#include <dlfcn.h>

#include <stdbool.h>
#include <pthread.h>

#include <sys/time.h>

#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"


typedef struct _event_runloop_info {
    CFMachPortRef port;
    CFRunLoopRef event;             // This is a reference to the thread that is being blocked by the event hook.
    CFRunLoopSourceRef source;
    CFRunLoopObserverRef observer;
} event_runloop_info;

// Structure for the current Unix epoch in milliseconds.
static struct timeval system_time;

// We define the event_runloop_info as a static so that hook_event_proc can
// re-enable the tap when it gets disabled by a timeout
static event_runloop_info *hook = NULL;

static bool keyboard = true;
static bool mouse = true;

static uint32_t ax_poll_frequency = 1;
static bool ax_access_revoked = false;

// The event mask to listen for.
static CGEventMask event_mask =
        CGEventMaskBit(kCGEventKeyDown) |
        CGEventMaskBit(kCGEventKeyUp) |
        CGEventMaskBit(kCGEventFlagsChanged) |

        CGEventMaskBit(kCGEventLeftMouseDown) |
        CGEventMaskBit(kCGEventLeftMouseUp) |
        CGEventMaskBit(kCGEventLeftMouseDragged) |

        CGEventMaskBit(kCGEventRightMouseDown) |
        CGEventMaskBit(kCGEventRightMouseUp) |
        CGEventMaskBit(kCGEventRightMouseDragged) |

        CGEventMaskBit(kCGEventOtherMouseDown) |
        CGEventMaskBit(kCGEventOtherMouseUp) |
        CGEventMaskBit(kCGEventOtherMouseDragged) |

        CGEventMaskBit(kCGEventMouseMoved) |
        CGEventMaskBit(kCGEventScrollWheel) |

        // NOTE This event is undocumented and used for caps-lock release and multi-media keys.
        CGEventMaskBit(NX_SYSDEFINED);


static uint64_t get_unix_timestamp() {
	// Get the local system time in UTC.
	gettimeofday(&system_time, NULL);

	// Convert the local system time to a Unix epoch in MS.
	uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

	return timestamp;
}

// Set the modifier mask to the current modifiers.
static void set_modifiers() {
    clear_modifier_mask();

    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Shift)) {
        set_modifier_mask(MASK_SHIFT_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightShift)) {
        set_modifier_mask(MASK_SHIFT_R);
    }

    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Control)) {
        set_modifier_mask(MASK_CTRL_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightControl)) {
        set_modifier_mask(MASK_CTRL_R);
    }

    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Option)) {
        set_modifier_mask(MASK_ALT_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightOption)) {
        set_modifier_mask(MASK_ALT_R);
    }

    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Command)) {
        set_modifier_mask(MASK_META_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightCommand)) {
        set_modifier_mask(MASK_META_R);
    }

    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_LBUTTON)) {
        set_modifier_mask(MASK_BUTTON1);
    }
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_RBUTTON)) {
        set_modifier_mask(MASK_BUTTON2);
    }

    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_MBUTTON)) {
        set_modifier_mask(MASK_BUTTON3);
    }
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_XBUTTON1)) {
        set_modifier_mask(MASK_BUTTON4);
    }
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_XBUTTON2)) {
        set_modifier_mask(MASK_BUTTON5);
    }

    if (CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState) & kCGEventFlagMaskAlphaShift) {
        set_modifier_mask(MASK_CAPS_LOCK);
    }

    // Best I can tell, macOS does not support Num or Scroll lock.
}

static CGEventRef hook_event_proc(CGEventTapProxy tap_proxy, CGEventType type, CGEventRef event_ref, void *refcon) {
    set_modifiers();

    bool consumed = false;
    uint64_t timestamp = get_unix_timestamp();

    // Get the event class.
    switch (type) {
        case kCGEventKeyDown:
            if (keyboard) {
                consumed = dispatch_key_press(timestamp, event_ref);
            }
            break;

        case kCGEventKeyUp:
            if (keyboard) {
                consumed = dispatch_key_release(timestamp, event_ref);
            }
            break;

        case kCGEventFlagsChanged:
            if (keyboard) {
                consumed = dispatch_modifier_change(timestamp, event_ref);
            }
            break;

        case NX_SYSDEFINED:
            if (keyboard) {
                consumed = dispatch_system_key(timestamp, event_ref);
            }
            break;

        case kCGEventLeftMouseDown:
            if (mouse) {
                set_modifier_mask(MASK_BUTTON1);
                consumed = dispatch_button_press(timestamp, event_ref, MOUSE_BUTTON1);
            }
            break;

        case kCGEventRightMouseDown:
            if (mouse) {
                set_modifier_mask(MASK_BUTTON2);
                consumed = dispatch_button_press(timestamp, event_ref, MOUSE_BUTTON2);
            }
            break;

        case kCGEventOtherMouseDown:
            // Extra mouse buttons.
            if (mouse && CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) < UINT16_MAX) {
                uint16_t button = (uint16_t) CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    set_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    set_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_press(timestamp, event_ref, button);
            }
            break;

        case kCGEventLeftMouseUp:
            if (mouse) {
                unset_modifier_mask(MASK_BUTTON1);
                consumed = dispatch_button_release(timestamp, event_ref, MOUSE_BUTTON1);
            }
            break;

        case kCGEventRightMouseUp:
            if (mouse) {
                unset_modifier_mask(MASK_BUTTON2);
                consumed = dispatch_button_release(timestamp, event_ref, MOUSE_BUTTON2);
            }
            break;

        case kCGEventOtherMouseUp:
            // Extra mouse buttons.
            if (mouse && CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) < UINT16_MAX) {
                uint16_t button = (uint16_t) CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    unset_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    unset_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_release(timestamp, event_ref, button);
            }
            break;


        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:
        case kCGEventOtherMouseDragged:
            // FIXME The drag flag is confusing.  Use prev x,y to determine click.
            if (mouse) {
                // Set the mouse dragged flag.
                set_mouse_dragged(true);
                consumed = dispatch_mouse_move(timestamp, event_ref);
            }
            break;

        case kCGEventMouseMoved:
            if (mouse) {
                // Set the mouse dragged flag.
                set_mouse_dragged(false);
                consumed = dispatch_mouse_move(timestamp, event_ref);
            }
            break;


        case kCGEventScrollWheel:
            if (mouse) {
                consumed = dispatch_mouse_wheel(timestamp, event_ref);
            }
            break;

        case kCGEventTapDisabledByTimeout:
            // Check for an old OS X bug where the tap seems to timeout for no reason.
            // See: http://stackoverflow.com/questions/2969110/cgeventtapcreate-breaks-down-mysteriously-with-key-down-events#2971217
            logger(LOG_LEVEL_WARN, "%s [%u]: CGEventTap timeout!\n",
                    __FUNCTION__, __LINE__);

            // We need to re-enable the tap
            if (hook->port) {
                CGEventTapEnable(hook->port, true);
            }
            break;

        default:
            // In theory this *should* never execute.
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Unhandled macOS event: %#X.\n",
                    __FUNCTION__, __LINE__, (unsigned int) type);
            break;
    }

    CGEventRef result_ref = NULL;
    if (!consumed) {
        result_ref = event_ref;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Consuming the current event. (%#X) (%#p)\n",
                __FUNCTION__, __LINE__, type, event_ref);
    }

    return result_ref;
}

static void hook_status_proc(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
	uint64_t timestamp = get_unix_timestamp();

    switch (activity) {
        case kCFRunLoopEntry:
            dispatch_hook_enabled(timestamp);
            break;

        case kCFRunLoopExit:
            dispatch_hook_disabled(timestamp);
            break;

        default:
            logger(LOG_LEVEL_WARN, "%s [%u]: Unhandled RunLoop activity! (%#X)\n",
                    __FUNCTION__, __LINE__, (unsigned int) activity);
    }
}

static CGEventRef dummy_hook_event_proc(CGEventTapProxy tap_proxy, CGEventType type, CGEventRef event_ref, void *param) {
    return event_ref;
}

static void *ax_status_proc(void *param) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Starting polling for Accessibility API access\n",
            __FUNCTION__, __LINE__);

    while (hook != NULL) {
        sleep(ax_poll_frequency);

        CFMachPortRef port = CGEventTapCreate(
                kCGSessionEventTap,
                kCGHeadInsertEventTap,
                kCGEventTapOptionDefault,
                event_mask,
                dummy_hook_event_proc,
                NULL);

        if (port == NULL) {
            ax_access_revoked = true;
            hook_stop();
        } else {
            CFMachPortInvalidate(port);
            CFRelease(port);
        }
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Finished polling for Accessibility API access\n",
            __FUNCTION__, __LINE__);

    return NULL;
}

static int create_event_runloop_info(event_runloop_info **hook) {
    if (*hook != NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Expected unallocated event_runloop_info pointer!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }

    // Try and allocate memory for event_runloop_info.
    *hook = calloc(1, sizeof(event_runloop_info));
    if (*hook == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for event_runloop_info structure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    // Create the event tap.
    (*hook)->port = CGEventTapCreate(
            kCGSessionEventTap,       // kCGHIDEventTap
            kCGHeadInsertEventTap,    // kCGTailAppendEventTap
            kCGEventTapOptionDefault, // kCGEventTapOptionListenOnly See https://github.com/kwhat/jnativehook/issues/22
            event_mask,
            hook_event_proc,
            NULL);
    if ((*hook)->port == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create event port!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_EVENT_PORT;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CGEventTapCreate Successful.\n",
                __FUNCTION__, __LINE__);
    }

    (*hook)->event = CFRunLoopGetCurrent();
    if ((*hook)->event == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopGetCurrent failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_GET_RUNLOOP;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopGetCurrent successful.\n",
                __FUNCTION__, __LINE__);
    }

    // Create the runloop event source from the event tap.
    (*hook)->source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, (*hook)->port, 0);
    if ((*hook)->source == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFMachPortCreateRunLoopSource failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFMachPortCreateRunLoopSource successful.\n",
                __FUNCTION__, __LINE__);
    }

    // Create run loop observers.
    (*hook)->observer = CFRunLoopObserverCreate(
            kCFAllocatorDefault,
            kCFRunLoopEntry | kCFRunLoopExit, //kCFRunLoopAllActivities,
            true,
            0,
            hook_status_proc,
            NULL);
    if ((*hook)->observer == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopObserverCreate failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_OBSERVER;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopObserverCreate successful.\n",
                __FUNCTION__, __LINE__);
    }

    // Add the event source and observer to the runloop mode.
    CFRunLoopAddSource((*hook)->event, (*hook)->source, kCFRunLoopDefaultMode);
    CFRunLoopAddObserver((*hook)->event, (*hook)->observer, kCFRunLoopDefaultMode);

    ax_access_revoked = false;
    pthread_t ax_access_thread;
    pthread_create(&ax_access_thread, NULL, &ax_status_proc, NULL);

    return UIOHOOK_SUCCESS;
}

static void destroy_event_runloop_info(event_runloop_info **hook) {
    if (*hook != NULL) {
        if ((*hook)->observer != NULL) {
            if (CFRunLoopContainsObserver((*hook)->event, (*hook)->observer, kCFRunLoopDefaultMode)) {
                CFRunLoopRemoveObserver((*hook)->event, (*hook)->observer, kCFRunLoopDefaultMode);
            }

            // Invalidate and free hook observer.
            CFRunLoopObserverInvalidate((*hook)->observer);
            CFRelease((*hook)->observer);
            (*hook)->observer = NULL;
        }

        if ((*hook)->source != NULL) {
            if (CFRunLoopContainsSource((*hook)->event, (*hook)->source, kCFRunLoopDefaultMode)) {
                CFRunLoopRemoveSource((*hook)->event, (*hook)->source, kCFRunLoopDefaultMode);
            }

            // Clean up the event source.
            CFRelease((*hook)->source);
            (*hook)->source = NULL;
        }

        if ((*hook)->event != NULL) {
            (*hook)->event = NULL;
        }

        if ((*hook)->port != NULL) {
            // Stop the CFMachPort from receiving any more messages.
            CFMachPortInvalidate((*hook)->port);
            CFRelease((*hook)->port);
            (*hook)->port = NULL;
        }

        // Free the hook structure.
        free(*hook);
        *hook = NULL;
    }
}

static int run() {
    // Check for accessibility before we start the loop.
    if (!hook_is_ax_api_enabled(hook_get_prompt_user_if_ax_api_disabled())) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Accessibility API is disabled!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_AXAPI_DISABLED;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Accessibility API is enabled.\n",
                __FUNCTION__, __LINE__);
    }

    // Try and allocate memory for event_runloop_info.
    int event_runloop_status = create_event_runloop_info(&hook);
    if (event_runloop_status != UIOHOOK_SUCCESS) {
        destroy_event_runloop_info(&hook);
        return event_runloop_status;
    }

    // Initialize native input helper.
    int input_helper_status = load_input_helper();
    if (input_helper_status != UIOHOOK_SUCCESS) {
        unload_input_helper();
        return input_helper_status;
    }

    // Start the hook thread runloop.
    CFRunLoopRun();

    destroy_event_runloop_info(&hook);

    // Cleanup native input functions.
    unload_input_helper();

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Something, something, something, complete.\n",
            __FUNCTION__, __LINE__);

    return ax_access_revoked ? UIOHOOK_ERROR_AXAPI_REVOKED : UIOHOOK_SUCCESS;
}

uint32_t hook_get_ax_poll_frequency() {
    return ax_poll_frequency;
}

void hook_set_ax_poll_frequency(uint32_t frequency) {
    ax_poll_frequency = frequency;
}

int hook_run() {
    keyboard = true;
    mouse = true;
    return run();
}

int hook_run_keyboard() {
    keyboard = true;
    mouse = false;
    return run();
}

int hook_run_mouse() {
    keyboard = false;
    mouse = true;
    return run();
}

int hook_stop() {
    if (hook != NULL) {
        CFStringRef mode = CFRunLoopCopyCurrentMode(hook->event);
        if (mode == NULL) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopCopyCurrentMode failure!\n",
                    __FUNCTION__, __LINE__);

            return UIOHOOK_FAILURE;
        }
        CFRelease(mode);

        // Stop the run loop.
        CFRunLoopStop(hook->event);
    }

    return UIOHOOK_SUCCESS;
}
