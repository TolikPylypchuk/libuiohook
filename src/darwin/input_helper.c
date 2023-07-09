/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
 * https://github.com/kwhat/libuiohook/
 *
 * libUIOHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libUIOHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef USE_APPLICATION_SERVICES
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <dlfcn.h>
#include <stdbool.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

#ifdef USE_APPKIT
#include <objc/objc.h>
#include <objc/objc-runtime.h>
#endif


// Dynamic library loading for dispatch_sync_f to offload tasks that must run on the main runloop.
static struct dispatch_queue_s *dispatch_main_queue_s;
static void (*dispatch_sync_f_f)(dispatch_queue_t, void *, void (*function)(void *));

// Flag to check to see if we are in a mouse dragging state.
static bool mouse_dragged = false;

// Modifiers for tracking key masks.
static uint16_t modifier_mask = 0x0000;

#ifdef USE_APPLICATION_SERVICES
// Tracks the source and observer for the main runloop.
typedef struct _cf_runloop_info {
    CFRunLoopSourceRef source;
    CFRunLoopObserverRef observer;
} cf_runloop_info;
static cf_runloop_info *main_runloop_info = NULL;

// Current dead key state.
static UInt32 deadkey_state;

// Input source data for the keyboard.
static TISInputSourceRef prev_keyboard_layout = NULL;
#endif

// These are the structures used to pass messages to the main runloop.
typedef struct {
    CGEventRef event;
    UniChar *buffer;
    UniCharCount size;
    UniCharCount length;
} TISKeycodeMessage;

typedef struct {
    CGEventRef event;
    UInt32 subtype;
    UInt32 data1;
} TISObjCMessage;

#if defined(USE_APPLICATION_SERVICES)
// If we are using application services we need pthreads to synchronize main runloop execution.
#include <pthread.h>

// FIXME Should we be init these differently? https://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutex_init.html
static pthread_cond_t main_runloop_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t main_runloop_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Scancode lookup table
static const uint16_t scancode_keycode_table[][2] = {
/*  { scancode,                keycode                  }, */
    { VC_UNDEFINED,            kVK_Undefined            },
    { VC_A,                    kVK_ANSI_A               },
    { VC_S,                    kVK_ANSI_S               },
    { VC_D,                    kVK_ANSI_D               },
    { VC_F,                    kVK_ANSI_F               },
    { VC_H,                    kVK_ANSI_H               },
    { VC_G,                    kVK_ANSI_G               },
    { VC_Z,                    kVK_ANSI_Z               },
    { VC_X,                    kVK_ANSI_X               },
    { VC_C,                    kVK_ANSI_C               },
    { VC_V,                    kVK_ANSI_V               },
    { VC_SECTION,              kVK_ISO_Section          },
    { VC_B,                    kVK_ANSI_B               },
    { VC_Q,                    kVK_ANSI_Q               },
    { VC_W,                    kVK_ANSI_W               },
    { VC_E,                    kVK_ANSI_E               },
    { VC_R,                    kVK_ANSI_R               },
    { VC_Y,                    kVK_ANSI_Y               },
    { VC_T,                    kVK_ANSI_T               },
    { VC_1,                    kVK_ANSI_1               },
    { VC_2,                    kVK_ANSI_2               },
    { VC_3,                    kVK_ANSI_3               },
    { VC_4,                    kVK_ANSI_4               },
    { VC_6,                    kVK_ANSI_6               },
    { VC_5,                    kVK_ANSI_5               },
    { VC_EQUALS,               kVK_ANSI_Equal           },
    { VC_9,                    kVK_ANSI_9               },
    { VC_7,                    kVK_ANSI_7               },
    { VC_MINUS,                kVK_ANSI_Minus           },
    { VC_8,                    kVK_ANSI_8               },
    { VC_0,                    kVK_ANSI_0               },
    { VC_CLOSE_BRACKET,        kVK_ANSI_RightBracket    },
    { VC_O,                    kVK_ANSI_O               },
    { VC_U,                    kVK_ANSI_U               },
    { VC_OPEN_BRACKET,         kVK_ANSI_LeftBracket     },
    { VC_I,                    kVK_ANSI_I               },
    { VC_P,                    kVK_ANSI_P               },
    { VC_ENTER,                kVK_Return               },
    { VC_L,                    kVK_ANSI_L               },
    { VC_J,                    kVK_ANSI_J               },
    { VC_QUOTE,                kVK_ANSI_Quote           },
    { VC_K,                    kVK_ANSI_K               },
    { VC_SEMICOLON,            kVK_ANSI_Semicolon       },
    { VC_BACK_SLASH,           kVK_ANSI_Backslash       },
    { VC_COMMA,                kVK_ANSI_Comma           },
    { VC_SLASH,                kVK_ANSI_Slash           },
    { VC_N,                    kVK_ANSI_N               },
    { VC_M,                    kVK_ANSI_M               },
    { VC_PERIOD,               kVK_ANSI_Period          },
    { VC_TAB,                  kVK_Tab                  },
    { VC_SPACE,                kVK_Space                },
    { VC_BACK_QUOTE,           kVK_ANSI_Grave           },
    { VC_BACKSPACE,            kVK_Delete               },
    { VC_ESCAPE,               kVK_Escape               },
    { VC_META_R,               kVK_RightCommand         },
    { VC_META_L,               kVK_Command              },
    { VC_SHIFT_L,              kVK_Shift                },
    { VC_CAPS_LOCK,            kVK_CapsLock             },
    { VC_ALT_L,                kVK_Option               },
    { VC_CONTROL_L,            kVK_Control              },
    { VC_SHIFT_R,              kVK_RightShift           },
    { VC_ALT_R,                kVK_RightOption          },
    { VC_CONTROL_R,            kVK_RightControl         },
    { VC_FUNCTION,             kVK_Function             },
    { VC_F17,                  kVK_F17                  },
    { VC_KP_DECIMAL,           kVK_ANSI_KeypadDecimal   },
    { VC_KP_MULTIPLY,          kVK_ANSI_KeypadMultiply  },
    { VC_KP_ADD,               kVK_ANSI_KeypadPlus      },
    { VC_KP_CLEAR,             kVK_ANSI_KeypadClear     },
    { VC_VOLUME_UP,            kVK_VolumeUp             },
    { VC_VOLUME_DOWN,          kVK_VolumeDown           },
    { VC_VOLUME_MUTE,          kVK_Mute                 },
    { VC_KP_DIVIDE,            kVK_ANSI_KeypadDivide    },
    { VC_KP_ENTER,             kVK_ANSI_KeypadEnter     },
    { VC_KP_SUBTRACT,          kVK_ANSI_KeypadMinus     },
    { VC_F18,                  kVK_F18                  },
    { VC_F19,                  kVK_F19                  },
    { VC_KP_EQUALS,            kVK_ANSI_KeypadEquals    },
    { VC_KP_0,                 kVK_ANSI_Keypad0         },
    { VC_KP_1,                 kVK_ANSI_Keypad1         },
    { VC_KP_2,                 kVK_ANSI_Keypad2         },
    { VC_KP_3,                 kVK_ANSI_Keypad3         },
    { VC_KP_4,                 kVK_ANSI_Keypad4         },
    { VC_KP_5,                 kVK_ANSI_Keypad5         },
    { VC_KP_6,                 kVK_ANSI_Keypad6         },
    { VC_KP_7,                 kVK_ANSI_Keypad7         },
    { VC_F20,                  kVK_F20                  },
    { VC_KP_8,                 kVK_ANSI_Keypad8         },
    { VC_KP_9,                 kVK_ANSI_Keypad9         },
    { VC_YEN,                  kVK_JIS_Yen              },
    { VC_UNDERSCORE,           kVK_JIS_Underscore       },
    { VC_KP_SEPARATOR,         kVK_JIS_KeypadComma      },
    { VC_F5,                   kVK_F5                   },
    { VC_F6,                   kVK_F6                   },
    { VC_F7,                   kVK_F7                   },
    { VC_F3,                   kVK_F3                   },
    { VC_F8,                   kVK_F8                   },
    { VC_F9,                   kVK_F9                   },
    { VC_ALPHANUMERIC,         kVK_JIS_Eisu             },
    { VC_F11,                  kVK_F11                  },
    { VC_KANA,                 kVK_JIS_Kana             },
    { VC_F13,                  kVK_F13                  },
    { VC_F16,                  kVK_F16                  },
    { VC_F14,                  kVK_F14                  },
    { VC_F10,                  kVK_F10                  },
    { VC_CONTEXT_MENU,         kVK_ContextMenu          },
    { VC_F12,                  kVK_F12                  },
    { VC_F15,                  kVK_F15                  },
    { VC_HELP,                 kVK_Help                 },
    { VC_HOME,                 kVK_Home                 },
    { VC_PAGE_UP,              kVK_PageUp               },
    { VC_DELETE,               kVK_ForwardDelete        },
    { VC_F4,                   kVK_F4                   },
    { VC_END,                  kVK_End                  },
    { VC_F2,                   kVK_F2                   },
    { VC_PAGE_DOWN,            kVK_PageDown             },
    { VC_F1,                   kVK_F1                   },
    { VC_LEFT,                 kVK_LeftArrow            },
    { VC_RIGHT,                kVK_RightArrow           },
    { VC_DOWN,                 kVK_DownArrow            },
    { VC_UP,                   kVK_UpArrow              },
    { VC_POWER,                kVK_NX_Power             },
    { VC_MEDIA_EJECT,          kVK_NX_Eject             },
    { VC_MEDIA_PLAY,           kVK_MEDIA_Play           },
    { VC_MEDIA_NEXT,           kVK_MEDIA_Next           },
    { VC_MEDIA_PREVIOUS,       kVK_MEDIA_Previous       },
    { VC_CHANGE_INPUT_SOURCE,  kVK_ChangeInputSource    },
};

bool is_accessibility_enabled() {
    bool is_enabled = false;

    // Dynamically load the application services framework for examination.
    Boolean (*AXIsProcessTrustedWithOptions_t)(CFDictionaryRef);
    *(void **) (&AXIsProcessTrustedWithOptions_t) = dlsym(RTLD_DEFAULT, "AXIsProcessTrustedWithOptions");
    const char *dlError = dlerror();
    if (AXIsProcessTrustedWithOptions_t != NULL) {
        // Check for property CFStringRef kAXTrustedCheckOptionPrompt
        void ** kAXTrustedCheckOptionPrompt_t = dlsym(RTLD_DEFAULT, "kAXTrustedCheckOptionPrompt");

        dlError = dlerror();
        if (dlError != NULL) {
            // Could not load the AXIsProcessTrustedWithOptions function!
            logger(LOG_LEVEL_WARN, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        } else if (kAXTrustedCheckOptionPrompt_t != NULL) {
            // New accessibility API 10.9 and later.
            const void * keys[] = { *kAXTrustedCheckOptionPrompt_t };
            const void * values[] = { kCFBooleanTrue };

            CFDictionaryRef options = CFDictionaryCreate(
                    kCFAllocatorDefault,
                    keys,
                    values,
                    sizeof(keys) / sizeof(*keys),
                    &kCFCopyStringDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);

            is_enabled = (*AXIsProcessTrustedWithOptions_t)(options);
        }
    } else {
        if (dlError != NULL) {
            logger(LOG_LEVEL_WARN, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        }

        logger(LOG_LEVEL_DEBUG, "%s [%u]: AXIsProcessTrustedWithOptions not found.\n",
                __FUNCTION__, __LINE__);

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Falling back to AXAPIEnabled().\n",
                __FUNCTION__, __LINE__);
        
        // Old accessibility check 10.8 and older.
        Boolean (*AXAPIEnabled_f)();
        *(void **) (&AXAPIEnabled_f) = dlsym(RTLD_DEFAULT, "AXAPIEnabled");
        dlError = dlerror();
        if (dlError != NULL) {
            // Could not load the AXIsProcessTrustedWithOptions function!
            logger(LOG_LEVEL_WARN, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        } else if (AXAPIEnabled_f != NULL) {
            is_enabled = (*AXAPIEnabled_f)();
        }
    }

    return is_enabled;
}


bool is_mouse_dragged() {
    return mouse_dragged;
}

void set_mouse_dragged(bool dragged) {
    mouse_dragged = dragged;
}


// Set the native modifier mask for future events.
void set_modifier_mask(uint16_t mask) {
    modifier_mask |= mask;
}

// Unset the native modifier mask for future events.
void unset_modifier_mask(uint16_t mask) {
    modifier_mask &= ~mask;
}

// Get the current native modifier mask state.
uint16_t get_modifiers() {
    return modifier_mask;
}

// Initialize the modifier mask to the current modifiers.
static void initialize_modifiers() {
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
    // Best I can tell, OS X does not support Num or Scroll lock.
    unset_modifier_mask(MASK_NUM_LOCK);
    unset_modifier_mask(MASK_SCROLL_LOCK);
}


uint16_t keycode_to_scancode(UInt64 keycode) {
    uint16_t scancode = VC_UNDEFINED;

    for (unsigned int i = 0; i < sizeof(scancode_keycode_table) / sizeof(scancode_keycode_table[0]); i++) {
        if (keycode == scancode_keycode_table[i][1]) {
            scancode = scancode_keycode_table[i][0];
            break;
        }
    }

    return scancode;
}

UInt64 scancode_to_keycode(uint16_t scancode) {
    UInt64 keycode = kVK_Undefined;

    for (unsigned int i = 0; i < sizeof(scancode_keycode_table) / sizeof(scancode_keycode_table[0]); i++) {
        if (scancode == scancode_keycode_table[i][0]) {
            keycode = scancode_keycode_table[i][1];
            break;
        }
    }

    return keycode;
}


static void tis_message_to_nsevent(void *info) {
    TISObjCMessage *tis = (TISObjCMessage *) info;

    if (tis != NULL && tis->event != NULL) {
        tis->subtype = 0;
        tis->data1 = 0;

        if (CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
            #ifdef USE_APPKIT
            // NOTE The following block must execute on the main runloop,
            // Ex: CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain()) to avoid "Exception detected while handling key input"
            // and "TSMProcessRawKeyCode failed (-192)" errors.
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using objc_msgSend for system key events.\n",
                    __FUNCTION__, __LINE__);

            // Contributed by Iván Munsuri Ibáñez <munsuri@gmail.com> and Alex <universailp@web.de>
            id (*eventWithCGEvent)(id, SEL, CGEventRef) = (id (*)(id, SEL, CGEventRef)) objc_msgSend;
            id event_data = eventWithCGEvent((id) objc_getClass("NSEvent"), sel_registerName("eventWithCGEvent:"), tis->event);

            UInt32 (*eventWithoutCGEvent)(id, SEL) = (UInt32 (*)(id, SEL)) objc_msgSend;
            tis->subtype = eventWithoutCGEvent(event_data, sel_registerName("subtype"));
            tis->data1 = eventWithoutCGEvent(event_data, sel_registerName("data1"));
            #else
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using CFDataGetBytes for system key events.\n",
                    __FUNCTION__, __LINE__);

            // If we are not using ObjC, the only way I've found to access CGEvent->subtype and CGEvent>data1 is to
            // serialize the event and read the byte offsets.  I am not sure why, but CGEventCreateData appears to use
            // big-endian byte ordering even though all current apple architectures are little-endian.
            CFDataRef data_ref = CGEventCreateData(kCFAllocatorDefault, tis->event);
            if (data_ref != NULL) {
                if (CFDataGetLength(data_ref) >= 132) {
                    UInt8 buffer[4];
                    CFDataGetBytes(data_ref, CFRangeMake(120, 4), &buffer);
                    tis->subtype = CFSwapInt32BigToHost(*((UInt32 *) &buffer));

                    CFDataGetBytes(data_ref, CFRangeMake(128, 4), &buffer);
                    tis->data1 = CFSwapInt32BigToHost(*((UInt32 *) &buffer));

                    CFRelease(data_ref);
                } else {
                    CFRelease(data_ref);
                    logger(LOG_LEVEL_ERROR, "%s [%u]: Insufficient CFData range size!\n",
                            __FUNCTION__, __LINE__);
                }
            } else {
                logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for CGEventRef copy!\n",
                        __FUNCTION__, __LINE__);
            }
            #endif
        }
    }
}

#ifdef USE_APPLICATION_SERVICES
/* Wrapper for tis_message_to_nsevent with mutex locking for use with runloop context switching. */
static void main_runloop_objc_proc(void *info) {
    // Lock the msg_port mutex as we enter the main runloop.
    pthread_mutex_lock(&main_runloop_mutex);

    tis_message_to_nsevent(info);

    // Unlock the msg_port mutex to signal to the hook_thread that we have finished on the main runloop.
    pthread_cond_broadcast(&main_runloop_cond);
    pthread_mutex_unlock(&main_runloop_mutex);
}
#endif

void event_to_objc(CGEventRef event_ref, UInt32 *subtype, UInt32 *data1) {
    TISObjCMessage tis_objc_message = {
        .event = event_ref,
        .subtype = 0,
        .data1 = 0
    };

    if (!CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
        if (dispatch_sync_f_f != NULL && dispatch_main_queue_s != NULL) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using dispatch_sync_f for system key events.\n",
                    __FUNCTION__, __LINE__);

            (*dispatch_sync_f_f)(dispatch_main_queue_s, &tis_objc_message, &tis_message_to_nsevent);
        }
        #ifdef USE_APPLICATION_SERVICES
        else {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using CFRunLoopWakeUp for key typed events.\n",
                    __FUNCTION__, __LINE__);

            // Lock for code dealing with the main runloop.
            pthread_mutex_lock(&main_runloop_mutex);

            // Check to see if the main runloop is still running.
            CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain());
            if (mode != NULL) {
                CFRelease(mode);

                if (main_runloop_info != NULL) {
                    // Lookup the Unicode representation for this event.
                    CFRunLoopSourceContext *context = NULL;
                    CFRunLoopSourceGetContext(main_runloop_info->source, context);

                    if (context != NULL) {
                        // Setup the context for this action
                        context->info = &tis_objc_message;
                        context->perform = main_runloop_objc_proc;

                        // Signal the custom source and wakeup the main runloop.
                        CFRunLoopSourceSignal(main_runloop_info->source);
                        CFRunLoopWakeUp(CFRunLoopGetMain());

                        // Wait for a lock while the main runloop processes they key typed event.
                        pthread_cond_wait(&main_runloop_cond, &main_runloop_mutex);
                    } else {
                        logger(LOG_LEVEL_ERROR, "%s [%u]: context is null!\n",
                                __FUNCTION__, __LINE__);
                    }
                } else {
                     logger(LOG_LEVEL_ERROR, "%s [%u]: main_runloop_info is null!\n",
                             __FUNCTION__, __LINE__);
                 }
            } else {
                logger(LOG_LEVEL_WARN, "%s [%u]: Failed to signal main runloop!\n",
                        __FUNCTION__, __LINE__);
            }

            // Unlock for code dealing with the main runloop.
            pthread_mutex_unlock(&main_runloop_mutex);
        }
        #endif
    } else {
        // We are already on the main runloop, so no fancy context switching is required required.
        tis_message_to_nsevent(&tis_objc_message);

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Using no runloop for objc message events.\n",
                __FUNCTION__, __LINE__);
    }

    *subtype = tis_objc_message.subtype;
    *data1 = tis_objc_message.data1;
}


// Preform Unicode lookup from the main runloop vui dispatch_sync_f_f or application services runloop signaling.
static void tis_message_to_unicode(void *info) {
    TISKeycodeMessage *tis = (TISKeycodeMessage *) info;

     if (tis != NULL && tis->event != NULL) {
        tis->length = 0;

        #ifdef USE_APPLICATION_SERVICES
        if (CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
            // NOTE The following block must execute on the main runloop,
            // Ex: CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain()) to avoid "Exception detected while handling key input"
            // and "TSMProcessRawKeyCode failed (-192)" errors.
            TISInputSourceRef curr_keyboard_layout = TISCopyCurrentKeyboardLayoutInputSource();
            if (curr_keyboard_layout != NULL && CFGetTypeID(curr_keyboard_layout) == TISInputSourceGetTypeID()) {

                const CFDataRef data = (CFDataRef) TISGetInputSourceProperty(curr_keyboard_layout, kTISPropertyUnicodeKeyLayoutData);
                if (data != NULL && CFGetTypeID(data) == CFDataGetTypeID() && CFDataGetLength(data) > 0) {

                    const UCKeyboardLayout *keyboard_layout = (const UCKeyboardLayout *) CFDataGetBytePtr(data);
                    if (keyboard_layout != NULL) {
                        //Extract keycode and modifier information.
                        CGKeyCode keycode = CGEventGetIntegerValueField(tis->event, kCGKeyboardEventKeycode);
                        CGEventFlags modifiers = CGEventGetFlags(tis->event);

                        // Disable all command modifiers for translation.  This is required
                        // so UCKeyTranslate will provide a keysym for the separate event.
                        static const CGEventFlags cmd_modifiers = kCGEventFlagMaskCommand | kCGEventFlagMaskControl | kCGEventFlagMaskAlternate;
                        modifiers &= ~cmd_modifiers;

                        // I don't know why but UCKeyTranslate does not process the
                        // kCGEventFlagMaskAlphaShift (A.K.A. Caps Lock Mask) correctly.
                        // We need to basically turn off the mask and process the capital
                        // letters after UCKeyTranslate().
                        bool is_caps_lock = modifiers & kCGEventFlagMaskAlphaShift;
                        modifiers &= ~kCGEventFlagMaskAlphaShift;

                        // Run the translation with the saved deadkey_state.
                        OSStatus status = UCKeyTranslate(
                                keyboard_layout,
                                keycode,
                                kUCKeyActionDown, //kUCKeyActionDisplay,
                                (modifiers >> 16) & 0xFF, //(modifiers >> 16) & 0xFF, || (modifiers >> 8) & 0xFF,
                                LMGetKbdType(),
                                kNilOptions, //kNilOptions, //kUCKeyTranslateNoDeadKeysMask
                                &deadkey_state,
                                tis->size,
                                &(tis->length),
                                tis->buffer);

                        if (status == noErr && tis->length > 0) {
                            if (is_caps_lock) {
                                // We *had* a caps lock mask so we need to convert to uppercase.
                                CFMutableStringRef keytxt = CFStringCreateMutableWithExternalCharactersNoCopy(
                                    kCFAllocatorDefault, tis->buffer, tis->length, tis->size, kCFAllocatorNull
                                );

                                if (keytxt != NULL) {
                                    CFLocaleRef locale = CFLocaleCopyCurrent();
                                    CFStringUppercase(keytxt, locale);
                                    CFRelease(locale);
                                    CFRelease(keytxt);
                                } else {
                                    // There was an problem creating the CFMutableStringRef.
                                    tis->length = 0;
                                }
                            }
                        } else {
                            // Make sure the tis->buffer tis->length is zero if an error occurred.
                            tis->length = 0;
                        }
                    }

                }
            }

            // Check if the keyboard layout has changed to see if the dead key state needs to be discarded.
            if (prev_keyboard_layout != NULL && curr_keyboard_layout != NULL && CFEqual(curr_keyboard_layout, prev_keyboard_layout) == false) {
                deadkey_state = 0x00;
            }

            // Release the previous keyboard layout.
            if (prev_keyboard_layout != NULL) {
                CFRelease(prev_keyboard_layout);
                prev_keyboard_layout = NULL;
            }

            // Set the previous keyboard layout to the current layout.
            if (curr_keyboard_layout != NULL) {
                prev_keyboard_layout = curr_keyboard_layout;
            }
        }
        #else
        CGEventKeyboardGetUnicodeString(tis->event, tis->size, &(tis->length), tis->buffer);
        #endif

        // The following codes should not be processed because they are invalid.
        if (tis->length == 1) {
            switch (tis->buffer[0]) {
                case 0x01: // Home
                case 0x04: // End
                case 0x05: // Help Key
                case 0x10: // Function Keys
                case 0x0B: // Page Up
                case 0x0C: // Page Down
                case 0x1F: // Volume Up
                    tis->length = 0;
            }
        }
    }
}

#ifdef USE_APPLICATION_SERVICES
/* Wrapper for tis_message_to_unicode with mutex locking for use with runloop context switching. */
static void main_runloop_unicode_proc(void *info) {
    // Lock the msg_port mutex as we enter the main runloop.
    pthread_mutex_lock(&main_runloop_mutex);

    tis_message_to_unicode(info);

    // Unlock the msg_port mutex to signal to the hook_thread that we have finished on the main runloop.
    pthread_cond_broadcast(&main_runloop_cond);
    pthread_mutex_unlock(&main_runloop_mutex);
}
#endif

UniCharCount event_to_unicode(CGEventRef event_ref, UniChar *buffer, UniCharCount size) {
    TISKeycodeMessage tis_keycode_message = {
        .event = event_ref,
        .buffer = buffer,
        .size = size,
        .length = 0
    };

    if (!CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
        if (dispatch_sync_f_f != NULL && dispatch_main_queue_s != NULL) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using dispatch_sync_f for key typed events.\n",
                    __FUNCTION__, __LINE__);
            (*dispatch_sync_f_f)(dispatch_main_queue_s, &tis_keycode_message, &tis_message_to_unicode);
        }
        #ifdef USE_APPLICATION_SERVICES
        else {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using CFRunLoopWakeUp for key typed events.\n",
                    __FUNCTION__, __LINE__);

            // Lock for code dealing with the main runloop.
            pthread_mutex_lock(&main_runloop_mutex);

            // Check to see if the main runloop is still running.
            CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain());
            if (mode != NULL) {
                CFRelease(mode);

                if (main_runloop_info != NULL) {
                    // Lookup the Unicode representation for this event.
                    CFRunLoopSourceContext *context = NULL;
                    CFRunLoopSourceGetContext(main_runloop_info->source, context);

                    if (context != NULL) {
                        // Setup the context for this action
                        context->info = &tis_keycode_message;
                        context->perform = main_runloop_unicode_proc;

                        // Signal the custom source and wakeup the main runloop.
                        CFRunLoopSourceSignal(main_runloop_info->source);
                        CFRunLoopWakeUp(CFRunLoopGetMain());

                        // Wait for a lock while the main runloop processes they key typed event.
                        pthread_cond_wait(&main_runloop_cond, &main_runloop_mutex);
                    } else {
                        logger(LOG_LEVEL_ERROR, "%s [%u]: context is null!\n",
                                __FUNCTION__, __LINE__);
                    }
                } else {
                     logger(LOG_LEVEL_ERROR, "%s [%u]: main_runloop_info is null!\n",
                             __FUNCTION__, __LINE__);
                 }
            } else {
                logger(LOG_LEVEL_WARN, "%s [%u]: Failed to signal main runloop!\n",
                        __FUNCTION__, __LINE__);
            }

            // Unlock for code dealing with the main runloop.
            pthread_mutex_unlock(&main_runloop_mutex);
        }
        #endif
    } else {
        // We are already on the main runloop, so no fancy context switching is required required.
        tis_message_to_unicode(&tis_keycode_message);

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Using no runloop for key typed events.\n",
                __FUNCTION__, __LINE__);
    }

    return tis_keycode_message.length;
}

#ifdef USE_APPLICATION_SERVICES
/* This is the callback for our cf_runloop_info.observer. */
void main_runloop_status_proc(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
    switch (activity) {
        case kCFRunLoopExit:
            // Acquire a lock on the msg_port and signal that anyone waiting should continue.
            pthread_mutex_lock(&main_runloop_mutex);
            pthread_cond_broadcast(&main_runloop_cond);
            pthread_mutex_unlock(&main_runloop_mutex);
            break;
    }
}

static int create_main_runloop_info(cf_runloop_info **main) {
    if (*main != NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Expected unallocated cf_runloop_info pointer!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }

    // Try and allocate memory for cf_runloop_info.
    *main = malloc(sizeof(cf_runloop_info));
    if (*main == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for cf_runloop_info structure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    // Allocate memory for the CFRunLoopSourceContext structure
    CFRunLoopSourceContext *context = (CFRunLoopSourceContext *) calloc(1, sizeof(CFRunLoopSourceContext));
    if (context == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for CFRunLoopSourceContext structure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    // Create a runloop observer for the main runloop.
    (*main)->observer = CFRunLoopObserverCreate(
            kCFAllocatorDefault,
            kCFRunLoopExit, //kCFRunLoopEntry | kCFRunLoopExit, //kCFRunLoopAllActivities,
            true,
            0,
            main_runloop_status_proc,
            NULL
        );
    if ((*main)->observer == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopObserverCreate failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_OBSERVER;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopObserverCreate success!\n",
                __FUNCTION__, __LINE__);
    }

    (*main)->source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, context);

    if ((*main)->source == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopSourceCreate failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopSourceCreate success!\n",
                __FUNCTION__, __LINE__);
    }

    CFRunLoopRef main_loop = CFRunLoopGetMain();

    pthread_mutex_lock(&main_runloop_mutex);

    CFRunLoopAddSource(main_loop, (*main)->source, kCFRunLoopDefaultMode);
    CFRunLoopAddObserver(main_loop, (*main)->observer, kCFRunLoopDefaultMode);

    pthread_mutex_unlock(&main_runloop_mutex);

    return UIOHOOK_SUCCESS;
}

static void destroy_main_runloop_info(cf_runloop_info **main) {
    if (*main != NULL) {
        CFRunLoopRef main_loop = CFRunLoopGetMain();

        if ((*main)->observer != NULL) {
            if (CFRunLoopContainsObserver(main_loop, (*main)->observer, kCFRunLoopDefaultMode)) {
                CFRunLoopRemoveObserver(main_loop, (*main)->observer, kCFRunLoopDefaultMode);
            }

            CFRunLoopObserverInvalidate((*main)->observer);
            CFRelease((*main)->observer);
            (*main)->observer = NULL;
        }

        if ((*main)->source != NULL) {
            // Lookup the Unicode representation for this event.
            CFRunLoopSourceContext *context = NULL;
            CFRunLoopSourceGetContext((*main)->source, context);

            if (context != NULL) {
                free(context);
            }

            if (CFRunLoopContainsSource(main_loop, (*main)->source, kCFRunLoopDefaultMode)) {
                CFRunLoopRemoveSource(main_loop, (*main)->source, kCFRunLoopDefaultMode);
            }

            CFRelease((*main)->source);
            (*main)->source = NULL;
        }

        // Free the main structure.
        free(*main);
        *main = NULL;
    }
}
#endif

int load_input_helper() {
    #ifdef USE_APPLICATION_SERVICES
    // Start with a fresh dead key state.
    deadkey_state = 0;
    #endif

    // Initialize the current state of the modifiers.
    initialize_modifiers();

    // If we are not running on the main runloop, we need to setup a runloop dispatcher.
    if (!CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
        // Dynamically load dispatch_sync_f to maintain 10.5 compatibility.
        *(void **) (&dispatch_sync_f_f) = dlsym(RTLD_DEFAULT, "dispatch_sync_f");
        const char *dlError = dlerror();
        if (dlError != NULL) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        }

        // This load is equivalent to calling dispatch_get_main_queue().  We use
        // _dispatch_main_q because dispatch_get_main_queue is not exported from
        // libdispatch.dylib and the upstream function only dereferences the pointer.
        dispatch_main_queue_s = (struct dispatch_queue_s *) dlsym(RTLD_DEFAULT, "_dispatch_main_q");
        dlError = dlerror();
        if (dlError != NULL) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        }

        if (dispatch_sync_f_f == NULL || dispatch_main_queue_s == NULL) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Failed to locate dispatch_sync_f() or dispatch_get_main_queue()!\n",
                    __FUNCTION__, __LINE__);

            #ifdef USE_APPLICATION_SERVICES
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Falling back to runloop signaling.\n",
                    __FUNCTION__, __LINE__);

            int keycode_runloop_status = create_main_runloop_info(&main_runloop_info);
            if (keycode_runloop_status != UIOHOOK_SUCCESS) {
                destroy_main_runloop_info(&main_runloop_info);
                return keycode_runloop_status;
            }
            #endif
        }
    }

    return UIOHOOK_SUCCESS;
}

void unload_input_helper() {
    #ifdef USE_APPLICATION_SERVICES
    if (!CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
        // TODO Are we using the right mutex type? PTHREAD_MUTEX_DEFAULT?
        // TODO See: https://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutexattr_settype.html
        // FIXME Need to check for errors on pthread_mutex_
        pthread_mutex_lock(&main_runloop_mutex);
        destroy_main_runloop_info(&main_runloop_info);
        pthread_mutex_unlock(&main_runloop_mutex);
    }
    #endif

    #ifdef USE_APPLICATION_SERVICES
    if (prev_keyboard_layout != NULL) {
        // Cleanup tracking of the previous layout.
        CFRelease(prev_keyboard_layout);
        prev_keyboard_layout = NULL;
    }
    #endif
}
