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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <uiohook.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#include "input_helper.h"
#include "logger.h"

static uint64_t post_text_delay = 50 * 1000000;

UIOHOOK_API uint64_t hook_get_post_text_delay_x11() {
    return post_text_delay;
}

UIOHOOK_API void hook_set_post_text_delay_x11(uint64_t delay) {
    post_text_delay = delay;
}

static int post_key_event(uiohook_event * const event) {
    load_key_mappings();
    KeyCode keycode = vcode_to_keycode(event->data.keyboard.keycode);
    if (keycode == 0x0000) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to lookup scancode: %li\n",
                __FUNCTION__, __LINE__, event->data.keyboard.keycode);
        return UIOHOOK_FAILURE;
    }

    Bool is_pressed;
    if (event->type == EVENT_KEY_PRESSED) {
        is_pressed = True;
    } else if (event->type == EVENT_KEY_RELEASED) {
        is_pressed = False;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid event for keyboard post event: %#X.\n",
                __FUNCTION__, __LINE__, event->type);
        return UIOHOOK_FAILURE;
    }

    if (!XTestFakeKeyEvent(helper_disp, keycode, is_pressed, 0)) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XTestFakeKeyEvent() failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int post_mouse_button_event(uiohook_event * const event) {
    XButtonEvent btn_event = {
        .serial = 0,
        .send_event = False,
        .display = helper_disp,

        .window = None,                                /* “event” window it is reported relative to */
        .root = None,                                  /* root window that the event occurred on */
        .subwindow = XDefaultRootWindow(helper_disp),  /* child window */

        .time = CurrentTime,

        .x = event->data.mouse.x,                      /* pointer x, y coordinates in event window */
        .y = event->data.mouse.y,

        .x_root = 0,                                   /* coordinates relative to root */
        .y_root = 0,

        .state = 0x00,                                 /* key or button mask */
        .same_screen = True
    };

    if (!(event->type == EVENT_MOUSE_PRESSED_IGNORE_COORDS || event->type == EVENT_MOUSE_RELEASED_IGNORE_COORDS)) {
        // Move the pointer to the specified position.
        XTestFakeMotionEvent(btn_event.display, -1, btn_event.x, btn_event.y, 0);
    }

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_PRESSED_IGNORE_COORDS:
            if (event->data.mouse.button < MOUSE_BUTTON1 || event->data.mouse.button > MOUSE_BUTTON5) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Invalid button specified for mouse pressed event! (%u)\n",
                        __FUNCTION__, __LINE__, event->data.mouse.button);
                return UIOHOOK_FAILURE;
            }

            if (XTestFakeButtonEvent(helper_disp, event->data.mouse.button, True, 0) != 0) {
                status = UIOHOOK_SUCCESS;
            }
            break;

        case EVENT_MOUSE_RELEASED:
        case EVENT_MOUSE_RELEASED_IGNORE_COORDS:
            if (event->data.mouse.button < MOUSE_BUTTON1 || event->data.mouse.button > MOUSE_BUTTON5) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Invalid button specified for mouse released event! (%u)\n",
                        __FUNCTION__, __LINE__, event->data.mouse.button);
                return UIOHOOK_FAILURE;
            }

            if (XTestFakeButtonEvent(helper_disp, event->data.mouse.button, False, 0) != 0) {
                status = UIOHOOK_SUCCESS;
            }
            break;

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid mouse button event: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    return status;
}

static int post_mouse_wheel_event(uiohook_event * const event) {
    int status = UIOHOOK_FAILURE;

    XButtonEvent btn_event = {
        .serial = 0,
        .send_event = False,
        .display = helper_disp,

        .window = None,                                /* “event” window it is reported relative to */
        .root = None,                                  /* root window that the event occurred on */
        .subwindow = XDefaultRootWindow(helper_disp),  /* child window */

        .time = CurrentTime,

        .x = event->data.wheel.x,                      /* pointer x, y coordinates in event window */
        .y = event->data.wheel.y,

        .x_root = 0,                                   /* coordinates relative to root */
        .y_root = 0,

        .state = 0x00,                                 /* key or button mask */
        .same_screen = True
    };

    // Wheel events should be the same as click events on X11.

    uint8_t wheel_button = 0;

    if (event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION) {
        wheel_button = event->data.wheel.rotation > 0 ? WheelRight : WheelLeft;
    } else {
        wheel_button = event->data.wheel.rotation > 0 ? WheelUp : WheelDown;
    }

    unsigned int button = button_map_lookup(wheel_button);

    if (XTestFakeButtonEvent(helper_disp, button, True, 0) != 0) {
        status = UIOHOOK_SUCCESS;
    }

    if (status == UIOHOOK_SUCCESS && XTestFakeButtonEvent(helper_disp, button, False, 0) == 0) {
        status = UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int post_mouse_motion_event(uiohook_event * const event) {
    int status = UIOHOOK_FAILURE;

    if (event->type == EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR) {
        Window window;
        int x, y;
        unsigned int mask;
        if (XQueryPointer(helper_disp, XDefaultRootWindow(helper_disp), &window, &window, &x, &y, &x, &y, &mask) != 0) {
            if (XTestFakeMotionEvent(helper_disp, -1, x + event->data.mouse.x, y + event->data.mouse.y, 0) != 0) {
                status = UIOHOOK_SUCCESS;
            }
        }
    } else {
        if (XTestFakeMotionEvent(helper_disp, -1, event->data.mouse.x, event->data.mouse.y, 0) != 0) {
            status = UIOHOOK_SUCCESS;
        }
    }

    return status;
}

UIOHOOK_API int hook_post_event(uiohook_event * const event) {
    if (helper_disp == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    XLockDisplay(helper_disp);

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_KEY_PRESSED:
        case EVENT_KEY_RELEASED:
            status = post_key_event(event);
            break;

        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_RELEASED:
        case EVENT_MOUSE_PRESSED_IGNORE_COORDS:
        case EVENT_MOUSE_RELEASED_IGNORE_COORDS:
            status = post_mouse_button_event(event);
            break;

        case EVENT_MOUSE_WHEEL:
            status = post_mouse_wheel_event(event);
            break;

        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_DRAGGED:
        case EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR:
            status = post_mouse_motion_event(event);
            break;

        case EVENT_KEY_TYPED:
        case EVENT_MOUSE_CLICKED:

        case EVENT_HOOK_ENABLED:
        case EVENT_HOOK_DISABLED:

        default:
            logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring post event type %#X\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    XSync(helper_disp, True);
    XUnlockDisplay(helper_disp);

    return status;
}

int is_surrogate(uint16_t uc) {
    return (uc - 0xD800U) < 2048U;
}

int is_high_surrogate(uint16_t uc) {
    return (uc & 0xFFFFFC00) == 0xD800;
}

int is_low_surrogate(uint16_t uc) {
    return (uc & 0xFFFFFC00) == 0xDC00;
}

uint32_t surrogate_to_utf32(uint16_t high, uint16_t low) {
    return (high << 10) + low - 0x35FDC00;
}

uint32_t *convert_utf16_to_utf32(const uint16_t * input, size_t count) {
    const uint16_t * const end = input + count;
    uint32_t *result = (uint32_t*)calloc(count + 1, sizeof(uint32_t));
    uint32_t *output = result;

    while (input < end) {
        const uint16_t uc = *input++;
        if (!is_surrogate(uc)) {
            *output++ = uc;
        } else {
            *output++ = is_high_surrogate(uc) && input < end && is_low_surrogate(*input)
                ? surrogate_to_utf32(uc, *input++)
                : 0xFFFD;
        }
    }

    return result;
}

KeySym *map_to_keysyms(const uint16_t * const text, size_t count, size_t *keysym_count) {
    uint32_t *utf32_text = convert_utf16_to_utf32(text, count);

    KeySym *keysyms = (KeySym*)calloc(count, sizeof(KeySym));

    size_t i = 0;
    for (; utf32_text[i] != 0; i++) {
        char str[9] = { 0 };
        sprintf(str, "U%04X", utf32_text[i]);
        keysyms[i] = XStringToKeysym(str);
    }

    *keysym_count = i;
    free(utf32_text);

    return keysyms;
}

KeyCode find_unused_keycode() {
    int min_keycode = 0, max_keycode = 0;
    if (!XDisplayKeycodes(helper_disp, &min_keycode, &max_keycode)) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XDisplayKeycodes() failed!\n",
                __FUNCTION__, __LINE__);
        return 0;
    }

    size_t unused_keycodes_count = 0;

    for (KeyCode keycode = max_keycode; keycode >= min_keycode; keycode--) {
        int keysyms_per_keycode = 0;
        KeySym *keycode_keysyms = XGetKeyboardMapping(helper_disp, keycode, 1, &keysyms_per_keycode);
        int used = false;

        for (int i = 0; i < keysyms_per_keycode; i++) {
            if (keycode_keysyms[i] != NoSymbol) {
                used = true;
                break;
            }
        }

        if (!XFree(keycode_keysyms)) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: XFree() failed!\n",
                    __FUNCTION__, __LINE__);
            return 0;
        }

        if (!used) {
            return keycode;
        }
    }

    return 0;
}

int post_keysym(KeySym keysym, KeyCode keycode) {
    KeySym keysyms[4] = { keysym, keysym, keysym, keysym }; // Use the same KeySym for 4 shift levels
    int result = XChangeKeyboardMapping(helper_disp, keycode, 4, keysyms, 1);
    if (result != Success) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XChangeKeyboardMapping() failed! (%d)\n",
                __FUNCTION__, __LINE__, result);
        return UIOHOOK_FAILURE;
    }

    XSync(helper_disp, True);

    struct timespec ts = {
        .tv_sec = post_text_delay / 1000000000,
        .tv_nsec = post_text_delay % 1000000000
    };

    nanosleep(&ts, NULL);

    if (!XTestFakeKeyEvent(helper_disp, keycode, true, 0)) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XTestFakeKeyEvent() failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_FAILURE;
    }

    XSync(helper_disp, True);

    if (!XTestFakeKeyEvent(helper_disp, keycode, false, 0)) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XTestFakeKeyEvent() failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_FAILURE;
    }

    XSync(helper_disp, True);

    nanosleep(&ts, NULL);

    return UIOHOOK_SUCCESS;
}

UIOHOOK_API int hook_post_text(const uint16_t * const text) {
    if (text == NULL) {
        return UIOHOOK_ERROR_POST_TEXT_NULL;
    }

    if (helper_disp == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    XLockDisplay(helper_disp);

    size_t count = 0;

    for (int i = 0; text[i] != 0; i++) {
        count++;
    }

    KeyCode unused_keycode = find_unused_keycode();

    if (unused_keycode == 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Cannot find an unused key code!\n",
                __FUNCTION__, __LINE__);

        XUnlockDisplay(helper_disp);
        return UIOHOOK_FAILURE;
    }

    size_t keysym_count = 0;
    KeySym *keysyms = map_to_keysyms(text, count, &keysym_count);

    int status = UIOHOOK_SUCCESS;

    for (size_t i = 0; i < keysym_count; i++) {
        if (post_keysym(keysyms[i], unused_keycode) != UIOHOOK_SUCCESS) {
            status = UIOHOOK_FAILURE;
            break;
        }
    }

    free(keysyms);

    KeySym keysym = NoSymbol;
    int result = XChangeKeyboardMapping(helper_disp, unused_keycode, 1, &keysym, 1);
    if (result != Success) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XChangeKeyboardMapping() failed! (%d)\n",
                __FUNCTION__, __LINE__, result);
        return UIOHOOK_FAILURE;
    }

    XSync(helper_disp, True);
    XUnlockDisplay(helper_disp);

    return status;
}
