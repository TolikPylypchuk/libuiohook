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

#include <stdio.h>
#include <uiohook.h>
#include <windows.h>

#include "input_helper.h"
#include "logger.h"
#include "monitor_helper.h"

// Some buggy versions of MinGW and MSys do not include these constants in winuser.h.
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC         0
#define MAPVK_VSC_TO_VK         1
#define MAPVK_VK_TO_CHAR        2
#define MAPVK_VSC_TO_VK_EX      3
#endif
// Some buggy versions of MinGW and MSys only define this value for Windows
// versions >= 0x0600 (Windows Vista) when it should be 0x0500 (Windows 2000).
#ifndef MAPVK_VK_TO_VSC_EX
#define MAPVK_VK_TO_VSC_EX      4
#endif

#ifndef KEYEVENTF_SCANCODE
#define KEYEVENTF_EXTENDEDKEY   0x0001
#define KEYEVENTF_KEYUP         0x0002
#define KEYEVENTF_UNICODE       0x0004
#define KEYEVENTF_SCANCODE      0x0008
#endif

#ifndef KEYEVENTF_KEYDOWN
#define KEYEVENTF_KEYDOWN       0x0000
#endif

#define MAX_WINDOWS_COORD_VALUE (1 << 16)

// http://letcoderock.blogspot.fr/2011/10/sendinput-with-shift-key-not-work.html
// https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input?redirectedfrom=MSDN#extended-key-flag
static const uint16_t extend_key_table[] = {
    VC_ALT_R,
    VC_CONTROL_R,
    VC_INSERT,
    VC_DELETE,
    VC_HOME,
    VC_END,
    VC_PAGE_UP,
    VC_PAGE_DOWN,
    VC_UP,
    VC_DOWN,
    VC_LEFT,
    VC_RIGHT,
    VC_NUM_LOCK,
    VC_PRINT_SCREEN,
    VC_KP_ENTER,
    VC_KP_DIVIDE,
    VC_CANCEL
};

typedef struct {
    LONG x;
    LONG y;
} normalized_coordinate;

UIOHOOK_API uint64_t hook_get_post_text_delay_x11() {
    // Not applicable on Windows, so does nothing
    return 0;
}

UIOHOOK_API void hook_set_post_text_delay_x11(uint64_t delay) {
    // Not applicable on Windows, so does nothing
}

static LONG get_absolute_coordinate(LONG coordinate, int screen_size) {
    return MulDiv((int) coordinate, MAX_WINDOWS_COORD_VALUE, screen_size);
}

static normalized_coordinate normalize_coordinates(LONG x, LONG y, int screen_width, int screen_height, LARGESTNEGATIVECOORDINATES lnc) {
    x += abs(lnc.left);
    y += abs(lnc.top);

    // Prevent clicking 0 coordinates to prevent monitor flicker
    if (x == 0)
    {
        x++;
    }
    if (y == 0)
    {
        y++;
    }

    normalized_coordinate nc = {
            .x = get_absolute_coordinate(x, screen_width),
            .y = get_absolute_coordinate(y, screen_height)
    };

    return nc;
}

static int map_keyboard_event(uiohook_event * const event, INPUT * const input) {
    input->type = INPUT_KEYBOARD; // | KEYEVENTF_SCANCODE
    //input->ki.time = GetSystemTime();

    switch (event->type) {
        case EVENT_KEY_PRESSED:
            input->ki.dwFlags = KEYEVENTF_KEYDOWN;
            break;

        case EVENT_KEY_RELEASED:
            input->ki.dwFlags = KEYEVENTF_KEYUP;
            break;

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid event for keyboard event mapping: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            return UIOHOOK_FAILURE;
    }

    input->ki.wVk = (WORD) vcode_to_keycode(event->data.keyboard.keycode);
    if (input->ki.wVk == 0x0000) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to lookup scancode: %li\n",
                __FUNCTION__, __LINE__, event->data.keyboard.keycode);
        return UIOHOOK_FAILURE;
    }

    input->ki.wScan = MapVirtualKeyW(input->ki.wVk, MAPVK_VK_TO_VSC);

    if (event->mask & MASK_ALT) {
        input->ki.dwFlags |= KF_ALTDOWN;
    }

    for (int i = 0; i < sizeof(extend_key_table) / sizeof(uint16_t)
            && event->data.keyboard.keycode == extend_key_table[i]; i++) {
        input->ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }

    return UIOHOOK_SUCCESS;
}

static int map_mouse_event(uiohook_event * const event, INPUT * const input) {
    uint16_t screen_width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    uint16_t screen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    input->type = INPUT_MOUSE;
    input->mi.mouseData = 0;
    input->mi.dwExtraInfo = 0;
    input->mi.time = 0; // GetSystemTime();

    if (event->type != EVENT_MOUSE_WHEEL) {
        LARGESTNEGATIVECOORDINATES lnc = get_largest_negative_coordinates();

        normalized_coordinate nc;

        if (event->type == EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR) {
            POINT p;
            if (GetCursorPos(&p)) {
                nc = normalize_coordinates(p.x + event->data.mouse.x, p.y + event->data.mouse.y, screen_width, screen_height, lnc);
            }
        } else {
            nc = normalize_coordinates(event->data.mouse.x, event->data.mouse.y, screen_width, screen_height, lnc);
        }

        input->mi.dy = nc.y;
        input->mi.dx = nc.x;
    }

    switch (event->type) {
        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_PRESSED_IGNORE_COORDS:
            if (event->data.mouse.button == MOUSE_NOBUTTON) {
                logger(LOG_LEVEL_WARN, "%s [%u]: No button specified for mouse pressed event!\n",
                        __FUNCTION__, __LINE__);
                return UIOHOOK_FAILURE;
            } else if (event->data.mouse.button == MOUSE_BUTTON1) {
                input->mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            } else if (event->data.mouse.button == MOUSE_BUTTON2) {
                input->mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            } else if (event->data.mouse.button == MOUSE_BUTTON3) {
                input->mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
            } else {
                input->mi.dwFlags = MOUSEEVENTF_XDOWN;
                if (event->data.mouse.button == MOUSE_BUTTON4) {
                    input->mi.mouseData = XBUTTON1;
                } else if (event->data.mouse.button == MOUSE_BUTTON5) {
                    input->mi.mouseData = XBUTTON2;
                } else {
                    input->mi.mouseData = event->data.mouse.button - 3;
                }
            }

            if (event->type == EVENT_MOUSE_PRESSED) {
                // We need to move the mouse to the correct location prior to clicking.
                event->type = EVENT_MOUSE_MOVED;
                // TODO Remember to check the status here.
                hook_post_event(event);
                event->type = EVENT_MOUSE_PRESSED;
            }

            break;

        case EVENT_MOUSE_RELEASED:
        case EVENT_MOUSE_RELEASED_IGNORE_COORDS:
            if (event->data.mouse.button == MOUSE_NOBUTTON) {
                logger(LOG_LEVEL_WARN, "%s [%u]: No button specified for mouse released event!\n",
                        __FUNCTION__, __LINE__);
                return UIOHOOK_FAILURE;
            } else if (event->data.mouse.button == MOUSE_BUTTON1) {
                input->mi.dwFlags = MOUSEEVENTF_LEFTUP;
            } else if (event->data.mouse.button == MOUSE_BUTTON2) {
                input->mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            } else if (event->data.mouse.button == MOUSE_BUTTON3) {
                input->mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            } else {
                input->mi.dwFlags = MOUSEEVENTF_XUP;
                if (event->data.mouse.button == MOUSE_BUTTON4) {
                    input->mi.mouseData = XBUTTON1;
                } else if (event->data.mouse.button == MOUSE_BUTTON5) {
                    input->mi.mouseData = XBUTTON2;
                } else {
                    input->mi.mouseData = event->data.mouse.button - 3;
                }
            }

            if (event->type == EVENT_MOUSE_RELEASED) {
                // We need to move the mouse to the correct location prior to clicking.
                event->type = EVENT_MOUSE_MOVED;
                // TODO Remember to check the status here.
                hook_post_event(event);
                event->type = EVENT_MOUSE_RELEASED;
            }

            break;

        case EVENT_MOUSE_WHEEL:
            if (event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION) {
                input->mi.dwFlags = (DWORD)MOUSEEVENTF_HWHEEL;
                input->mi.mouseData = (DWORD)(event->data.wheel.rotation * -1);
            } else {
                input->mi.dwFlags = (DWORD)MOUSEEVENTF_WHEEL;
                input->mi.mouseData = (DWORD)event->data.wheel.rotation;
            }
            break;

        case EVENT_MOUSE_DRAGGED:
        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR:
            input->mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            break;

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid event for mouse event mapping: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            return UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

UIOHOOK_API int hook_post_event(uiohook_event * const event) {
    INPUT *input = (INPUT *) calloc(1, sizeof(INPUT));
    if (input == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: failed to allocate memory: calloc!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_KEY_PRESSED:
        case EVENT_KEY_RELEASED:
            status = map_keyboard_event(event, input);
            break;

        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_RELEASED:
        case EVENT_MOUSE_WHEEL:
        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR:
        case EVENT_MOUSE_DRAGGED:
        case EVENT_MOUSE_PRESSED_IGNORE_COORDS:
        case EVENT_MOUSE_RELEASED_IGNORE_COORDS:
            status = map_mouse_event(event, input);
            break;

        case EVENT_KEY_TYPED:
        case EVENT_MOUSE_CLICKED:

        case EVENT_HOOK_ENABLED:
        case EVENT_HOOK_DISABLED:

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Ignoring post event: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    if (status == UIOHOOK_SUCCESS && !SendInput(1, input, sizeof(INPUT))) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: SendInput() failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long) GetLastError());
        status = UIOHOOK_FAILURE;
    }

    free(input);

    return status;
}

UIOHOOK_API int hook_post_text(const uint16_t * const text) {
    if (text == NULL) {
        return UIOHOOK_ERROR_POST_TEXT_NULL;
    }

    int status = UIOHOOK_SUCCESS;

    size_t count = 0;

    for (int i = 0; text[i] != 0; i++) {
        count++;
    }

    INPUT *input = (INPUT*)calloc(count * 2, sizeof(INPUT));

    if (input == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: failed to allocate memory: calloc!\n",
            __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    for (int i = 0; i < count; i++) {
        input[i].type = INPUT_KEYBOARD;
        input[i].ki.wVk = 0;
        input[i].ki.wScan = (WORD)text[i];
        input[i].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYDOWN;
    }

    for (int i = 0; i < count; i++) {
        input[count + i].type = INPUT_KEYBOARD;
        input[count + i].ki.wVk = 0;
        input[count + i].ki.wScan = (WORD)text[i];
        input[count + i].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }

    if (!SendInput((UINT)count * 2, input, sizeof(INPUT))) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: SendInput() failed! (%#lX)\n",
            __FUNCTION__, __LINE__, (unsigned long)GetLastError());
        status = UIOHOOK_FAILURE;
    }

    free(input);

    return UIOHOOK_SUCCESS;
}
