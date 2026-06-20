#ifdef USE_APPLICATION_SERVICES
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <mach/mach_time.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

// Required to transport messages between the main runloop and our thread for Unicode look-ups.
#define KEY_BUFFER_SIZE 4

// Virtual event pointer.
static uiohook_event uio_event;

// Event dispatch callback.
static dispatcher_t dispatch = NULL;
static void *dispatch_data = NULL;

// Click count globals.
static unsigned short click_count = 0;
static CGEventTimestamp click_time = 0;
static unsigned short int click_button = MOUSE_NOBUTTON;

static bool key_typed_enabled = false;

bool hook_is_key_typed_enabled() {
    return key_typed_enabled;
}

void hook_set_key_typed_enabled(bool enabled) {
    key_typed_enabled = enabled;
}

void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting new dispatch callback to %#p.\n",
            __FUNCTION__, __LINE__, dispatch_proc);

    dispatch = dispatch_proc;
    dispatch_data = user_data;
}

// Send out an event if a dispatcher was set.
static void dispatch_event(uiohook_event *const event) {
    if (dispatch != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Dispatching event type %u.\n",
                __FUNCTION__, __LINE__, event->type);

        dispatch(event, dispatch_data);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: No dispatch callback set!\n",
                __FUNCTION__, __LINE__);
    }
}

bool dispatch_hook_enabled(uint64_t timestamp) {
    bool consumed = false;

    // Populate the hook start event.
    uio_event.time = timestamp;
    uio_event.type = EVENT_HOOK_ENABLED;
    uio_event.mask = 0x00;

    // Fire the hook start event.
    dispatch_event(&uio_event);
    consumed = uio_event.mask & MASK_CONSUMED;

    return consumed;
}

bool dispatch_hook_disabled(uint64_t timestamp) {
    bool consumed = false;

    // Populate the hook stop event.
    uio_event.time = timestamp;
    uio_event.type = EVENT_HOOK_DISABLED;
    uio_event.mask = 0x00;

    // Fire the hook stop event.
    dispatch_event(&uio_event);

    // Deinitialize native input helper functions.
    unload_input_helper();
    consumed = uio_event.mask & MASK_CONSUMED;

    return consumed;
}

bool dispatch_key_press(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    CGKeyCode keycode = (CGKeyCode) CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);

    // Check and setup modifiers.
    if      (keycode == kVK_Shift)        { set_modifier_mask(MASK_SHIFT_L);     }
    else if (keycode == kVK_RightShift)   { set_modifier_mask(MASK_SHIFT_R);     }
    else if (keycode == kVK_Control)      { set_modifier_mask(MASK_CTRL_L);      }
    else if (keycode == kVK_RightControl) { set_modifier_mask(MASK_CTRL_R);      }
    else if (keycode == kVK_Option)       { set_modifier_mask(MASK_ALT_L);       }
    else if (keycode == kVK_RightOption)  { set_modifier_mask(MASK_ALT_R);       }
    else if (keycode == kVK_Command)      { set_modifier_mask(MASK_META_L);      }
    else if (keycode == kVK_RightCommand) { set_modifier_mask(MASK_META_R);      }
    else if (keycode == kVK_CapsLock && (get_modifiers() & MASK_CAPS_LOCK) == 0) {
        set_modifier_mask(MASK_CAPS_LOCK);
    }

    // Populate key pressed event.
    uio_event.time = timestamp;
    uio_event.type = EVENT_KEY_PRESSED;
    uio_event.mask = get_modifiers();
    if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
        uio_event.mask |= MASK_EMULATED;
    }

    uint16_t uiocode = keycode_to_uiocode(keycode);

    uio_event.data.keyboard.keycode = uiocode;
    uio_event.data.keyboard.rawcode = keycode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X pressed. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key pressed event.
    dispatch_event(&uio_event);
    consumed = uio_event.mask & MASK_CONSUMED;

    // If the pressed event was not consumed and key typed events are enabled.
    if (key_typed_enabled && !consumed) {
        UniChar buffer[KEY_BUFFER_SIZE];
        UniCharCount length = event_to_unicode(event_ref, buffer, KEY_BUFFER_SIZE);

        for (unsigned int i = 0; i < length; i++) {
            // Populate key typed event.
            uio_event.time = timestamp;
            uio_event.type = EVENT_KEY_TYPED;
            uio_event.mask = get_modifiers();
            if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
                uio_event.mask |= MASK_EMULATED;
            }

            uio_event.data.keyboard.keycode = uiocode;
            uio_event.data.keyboard.rawcode = keycode;
            uio_event.data.keyboard.keychar = buffer[i];

            logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X typed. (%lc)\n",
                    __FUNCTION__, __LINE__,
                    uio_event.data.keyboard.keycode,
                    (wint_t) uio_event.data.keyboard.keychar);

            // Populate key typed event.
            dispatch_event(&uio_event);
            consumed = uio_event.mask & MASK_CONSUMED;
        }
    }

    return consumed;
}

bool dispatch_key_release(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    CGKeyCode keycode = (CGKeyCode) CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);

    // Check and setup modifiers.
    if      (keycode == kVK_Shift)        { unset_modifier_mask(MASK_SHIFT_L);      }
    else if (keycode == kVK_RightShift)   { unset_modifier_mask(MASK_SHIFT_R);      }
    else if (keycode == kVK_Control)      { unset_modifier_mask(MASK_CTRL_L);       }
    else if (keycode == kVK_RightControl) { unset_modifier_mask(MASK_CTRL_R);       }
    else if (keycode == kVK_Option)       { unset_modifier_mask(MASK_ALT_L);        }
    else if (keycode == kVK_RightOption)  { unset_modifier_mask(MASK_ALT_R);        }
    else if (keycode == kVK_Command)      { unset_modifier_mask(MASK_META_L);       }
    else if (keycode == kVK_RightCommand) { unset_modifier_mask(MASK_META_R);       }
    // Caps Lock should not be unset as it is already handled when setting
    // the modifier mask based on the macOS event.

    // Populate key released event.
    uio_event.time = timestamp;
    uio_event.type = EVENT_KEY_RELEASED;
    uio_event.mask = get_modifiers();
    if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
        uio_event.mask |= MASK_EMULATED;
    }

    uio_event.data.keyboard.keycode = keycode_to_uiocode(keycode);
    uio_event.data.keyboard.rawcode = keycode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X released. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key released event.
    dispatch_event(&uio_event);
    consumed = uio_event.mask & MASK_CONSUMED;

    return consumed;
}

bool dispatch_system_key_copy_event(uint64_t timestamp, CGEventRef event_ref, CGKeyCode key_code, bool key_down) {
    bool consumed = false;

    // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_CapsLock, key_down);
    CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));
    CGEventSetIntegerValueField(
            ns_event,
            kCGEventSourceUnixProcessID,
            CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID));

    if (key_down) {
        consumed = dispatch_key_press(timestamp, ns_event);
    } else {
        consumed = dispatch_key_release(timestamp, ns_event);
    }

    CFRelease(ns_event);
    CFRelease(src);

    return consumed;
}

bool dispatch_system_key(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    if (CGEventGetType(event_ref) == NX_SYSDEFINED) {
        UInt32 subtype = 0;
        UInt32 data1 = 0;

        // The subtype and data1 extraction needs to happen on the main runloop if objc is used.  This prevents an
        // error with the caps-lock key. See: https://github.com/kwhat/libuiohook/pull/108
        event_to_objc(event_ref, &subtype, &data1);

        if (subtype == 8) {
            int key_code = (data1 & 0xFFFF0000) >> 16;
            int key_flags = (data1 & 0xFFFF);
            int key_state = (key_flags & 0xFF00) >> 8;
            bool key_down = (key_state & 0x1) == 0;

            CGKeyCode adapted_key_code = 0;

            switch (key_code) {
                case NX_KEYTYPE_CAPS_LOCK:
                    adapted_key_code = kVK_CapsLock;
                    break;
                case NX_KEYTYPE_SOUND_UP:
                    adapted_key_code = kVK_VolumeUp;
                    break;
                case NX_KEYTYPE_SOUND_DOWN:
                    adapted_key_code = kVK_VolumeDown;
                    break;
                case NX_KEYTYPE_MUTE:
                    adapted_key_code = kVK_Mute;
                    break;
                case NX_KEYTYPE_EJECT:
                    adapted_key_code = kVK_NX_Eject;
                    break;
                case NX_KEYTYPE_PLAY:
                    adapted_key_code = kVK_MEDIA_Play;
                    break;
                case NX_KEYTYPE_FAST:
                    adapted_key_code = kVK_MEDIA_Next;
                    break;
                case NX_KEYTYPE_REWIND:
                    adapted_key_code = kVK_MEDIA_Previous;
                    break;
            }

            if (adapted_key_code) {
                consumed = dispatch_system_key_copy_event(timestamp, event_ref, adapted_key_code, key_down);
            }
        }
    }

    return consumed;
}

bool dispatch_modifier_change(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    CGEventFlags event_mask = CGEventGetFlags(event_ref);
    UInt64 keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Modifiers Changed for key %#X. (%#X)\n",
            __FUNCTION__, __LINE__, (unsigned long) keycode, (unsigned int) event_mask);

    /* Because Apple treats modifier keys differently than normal key
     * events, any changes to the modifier keys will require a key state
     * change to be fired manually.
     *
     * NOTE Left and right keyboard masks like NX_NEXTLSHIFTKEYMASK exist and
     * appear to be in use on Darwin, however they are removed by comment or
     * preprocessor with a note that reads "device-dependent (really?)."  To
     * ensure compatability, we will do this the verbose way.
     *
     * NOTE The masks for scroll and number lock are set in the key event.
     */
    if (keycode == kVK_Shift) {
        if (event_mask & kCGEventFlagMaskShift) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_SHIFT_L);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_SHIFT_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_Control) {
        if (event_mask & kCGEventFlagMaskControl) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_CTRL_L);
            dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_CTRL_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_Command) {
        if (event_mask & kCGEventFlagMaskCommand) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_META_L);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_META_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_Option) {
        if (event_mask & kCGEventFlagMaskAlternate) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_ALT_L);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_ALT_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightShift) {
        if (event_mask & kCGEventFlagMaskShift) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_SHIFT_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_SHIFT_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightControl) {
        if (event_mask & kCGEventFlagMaskControl) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_CTRL_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_CTRL_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightCommand) {
        if (event_mask & kCGEventFlagMaskCommand) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_META_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_META_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightOption) {
        if (event_mask & kCGEventFlagMaskAlternate) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_ALT_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_ALT_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_CapsLock) {
        if (get_modifiers() & MASK_CAPS_LOCK) {
            // Process as a key pressed event.
            unset_modifier_mask(MASK_CAPS_LOCK);
            // Key pressed handled by dispatch_system_key
        } else {
            // Process as a key released event.
            set_modifier_mask(MASK_CAPS_LOCK);
            // Key released handled by dispatch_system_key
        }
    }

    return consumed;
}

bool dispatch_button_press(uint64_t timestamp, CGEventRef event_ref, uint16_t button) {
    bool consumed = false;

    // Track the number of clicks.
    if (button == click_button && (long int) (timestamp - click_time) <= hook_get_multi_click_time()) {
        if (click_count < USHRT_MAX) {
            click_count++;
        }
        else {
            logger(LOG_LEVEL_WARN, "%s [%u]: Click count overflow detected!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        // Reset the click count.
        click_count = 1;

        // Set the previous button.
        click_button = button;
    }

    // Save this events time to calculate the click_count.
    click_time = timestamp;

    CGPoint event_point = CGEventGetLocation(event_ref);

    // Populate mouse pressed event.
    uio_event.time = timestamp;
    uio_event.type = EVENT_MOUSE_PRESSED;
    uio_event.mask = get_modifiers();
    if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
        uio_event.mask |= MASK_EMULATED;
    }

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click_count;
    uio_event.data.mouse.x = event_point.x;
    uio_event.data.mouse.y = event_point.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u pressed %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse pressed event.
    dispatch_event(&uio_event);
    consumed = uio_event.mask & MASK_CONSUMED;

    return consumed;
}

bool dispatch_button_release(uint64_t timestamp, CGEventRef event_ref, uint16_t button) {
    bool consumed = false;

    CGPoint event_point = CGEventGetLocation(event_ref);

    // Populate mouse released event.
    uio_event.time = timestamp;
    uio_event.type = EVENT_MOUSE_RELEASED;
    uio_event.mask = get_modifiers();
    if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
        uio_event.mask |= MASK_EMULATED;
    }

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click_count;
    uio_event.data.mouse.x = event_point.x;
    uio_event.data.mouse.y = event_point.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u released %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse released event.
    dispatch_event(&uio_event);
    consumed = uio_event.mask & MASK_CONSUMED;

    // If the pressed event was not consumed...
    if (!consumed && !is_mouse_dragged()) {
        // Populate mouse clicked event.
        uio_event.time = timestamp;
        uio_event.type = EVENT_MOUSE_CLICKED;
        uio_event.mask = get_modifiers();
        if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
            uio_event.mask |= MASK_EMULATED;
        }

        uio_event.data.mouse.button = button;
        uio_event.data.mouse.clicks = click_count;
        uio_event.data.mouse.x = event_point.x;
        uio_event.data.mouse.y = event_point.y;

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
                __FUNCTION__, __LINE__,
                uio_event.data.mouse.button, uio_event.data.mouse.clicks,
                uio_event.data.mouse.x, uio_event.data.mouse.y);

        // Fire mouse clicked event.
        dispatch_event(&uio_event);
        consumed = uio_event.mask & MASK_CONSUMED;
    }

    // Reset the number of clicks.
    if ((long int) (timestamp - click_time) > hook_get_multi_click_time()) {
        // Reset the click count.
        click_count = 0;
    }

    return consumed;
}

bool dispatch_mouse_move(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    // Reset the click count.
    if (click_count != 0 && (long int) (timestamp - click_time) > hook_get_multi_click_time()) {
        click_count = 0;
    }

    CGPoint event_point = CGEventGetLocation(event_ref);

    // Populate mouse motion event.
    uio_event.time = timestamp;
    if (is_mouse_dragged()) {
        uio_event.type = EVENT_MOUSE_DRAGGED;
    }
    else {
        uio_event.type = EVENT_MOUSE_MOVED;
    }
    uio_event.mask = get_modifiers();
    if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
        uio_event.mask |= MASK_EMULATED;
    }

    uio_event.data.mouse.button = MOUSE_NOBUTTON;
    uio_event.data.mouse.clicks = click_count;
    uio_event.data.mouse.x = event_point.x;
    uio_event.data.mouse.y = event_point.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse %s to %u, %u.\n",
            __FUNCTION__, __LINE__, is_mouse_dragged() ? "dragged" : "moved",
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse motion event.
    dispatch_event(&uio_event);
    consumed = uio_event.mask & MASK_CONSUMED;

    return consumed;
}

bool dispatch_mouse_wheel(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    // Reset the click count and previous button.
    click_count = 0;
    click_button = MOUSE_NOBUTTON;

    // Check to see what axis was rotated, we only care about axis 1 for vertical rotation.
    // NOTE kCGScrollWheelEventDeltaAxis3 is currently unused.
    if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0
            || CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis2) != 0) {
        CGPoint event_point = CGEventGetLocation(event_ref);

        // Populate mouse wheel event.
        uio_event.time = timestamp;
        uio_event.type = EVENT_MOUSE_WHEEL;
        uio_event.mask = get_modifiers();
        if (CGEventGetIntegerValueField(event_ref, kCGEventSourceUnixProcessID)) {
            uio_event.mask |= MASK_EMULATED;
        }

        uio_event.data.wheel.x = event_point.x;
        uio_event.data.wheel.y = event_point.y;

        uio_event.data.wheel.delta = 0;
        uio_event.data.wheel.rotation = 0;

        /* This function returns the scale of pixels per line in the specified event source. For example, if the
         * scale in the event source is 10.5 pixels per line, this function would return 10.5. Every scrolling event
         * can be interpreted to be scrolling by pixel or by line. By default, the scale is about ten pixels per
         * line. You can alter the scale with the function CGEventSourceSetPixelsPerLine.
         * See: https://gist.github.com/svoisen/5215826 */
        CGEventSourceRef source = CGEventCreateSourceFromEvent(event_ref);
        double ppl = CGEventSourceGetPixelsPerLine(source);

        if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventIsContinuous) != 0) {
            // continuous device (trackpad)
            ppl *= 1;
            uio_event.data.wheel.type = WHEEL_BLOCK_SCROLL;

            if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0) {
                uio_event.data.wheel.direction = WHEEL_VERTICAL_DIRECTION;
                uio_event.data.wheel.rotation = (int16_t) (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventPointDeltaAxis1) * ppl * 1);
            } else if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis2) != 0) {
                uio_event.data.wheel.direction = WHEEL_HORIZONTAL_DIRECTION;
                uio_event.data.wheel.rotation = (int16_t) (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventPointDeltaAxis2) * ppl * 1);
            }
        } else {
            // non-continuous device (wheel mice)
            ppl *= 10;
            uio_event.data.wheel.type = WHEEL_UNIT_SCROLL;

            if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0) {
                uio_event.data.wheel.direction = WHEEL_VERTICAL_DIRECTION;
                uio_event.data.wheel.rotation = (int16_t) (CGEventGetDoubleValueField(event_ref, kCGScrollWheelEventFixedPtDeltaAxis1) * ppl * 10);
            } else if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis2) != 0) {
                uio_event.data.wheel.direction = WHEEL_HORIZONTAL_DIRECTION;
                uio_event.data.wheel.rotation = (int16_t) (CGEventGetDoubleValueField(event_ref, kCGScrollWheelEventFixedPtDeltaAxis2) * ppl * 10);
            }
        }

        uio_event.data.wheel.delta = (uint16_t) ppl;

        if (source) {
            CFRelease(source);
        }

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse wheel %i / %u of type %u in the %u direction at %u, %u.\n",
                __FUNCTION__, __LINE__,
                uio_event.data.wheel.rotation,
                uio_event.data.wheel.delta,
                uio_event.data.wheel.type,
                uio_event.data.wheel.direction,
                uio_event.data.wheel.x,
                uio_event.data.wheel.y);

        // Fire mouse wheel event.
        dispatch_event(&uio_event);
        consumed = uio_event.mask & MASK_CONSUMED;
    }

    return consumed;
}
