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

#include <inttypes.h>
#include <uiohook.h>
#include <windows.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"
#include "monitor_helper.h"

// Thread and hook handles.
static DWORD hook_thread_id = 0;
static HHOOK keyboard_event_hhook = NULL, mouse_event_hhook = NULL;
static HWND invisible_win_hwnd = NULL;
static BOOL invisible_win_class_initialized = FALSE;

// The handle to the DLL module pulled in DllMain on DLL_PROCESS_ATTACH.
extern HINSTANCE hInst;

// Modifiers for tracking key masks.
static unsigned short int current_modifiers = 0x0000;

#ifdef USE_EPOCH_TIME
// Structure for the current Unix epoch in milliseconds.
static FILETIME system_time;
#endif

// Initialize the modifier mask to the current modifiers.
static void initialize_modifiers(bool keyboard, bool mouse) {
    current_modifiers = 0x0000;

    if (keyboard) {
        // NOTE We are checking the high order bit, so it will be < 0 for a singed short.
        if (GetKeyState(VK_LSHIFT)   < 0) { set_modifier_mask(MASK_SHIFT_L);     }
        if (GetKeyState(VK_RSHIFT)   < 0) { set_modifier_mask(MASK_SHIFT_R);     }
        if (GetKeyState(VK_LCONTROL) < 0) { set_modifier_mask(MASK_CTRL_L);      }
        if (GetKeyState(VK_RCONTROL) < 0) { set_modifier_mask(MASK_CTRL_R);      }
        if (GetKeyState(VK_LMENU)    < 0) { set_modifier_mask(MASK_ALT_L);       }
        if (GetKeyState(VK_RMENU)    < 0) { set_modifier_mask(MASK_ALT_R);       }
        if (GetKeyState(VK_LWIN)     < 0) { set_modifier_mask(MASK_META_L);      }
        if (GetKeyState(VK_RWIN)     < 0) { set_modifier_mask(MASK_META_R);      }

        if (GetKeyState(VK_NUMLOCK)  < 0) { set_modifier_mask(MASK_NUM_LOCK);    }
        if (GetKeyState(VK_CAPITAL)  < 0) { set_modifier_mask(MASK_CAPS_LOCK);   }
        if (GetKeyState(VK_SCROLL)   < 0) { set_modifier_mask(MASK_SCROLL_LOCK); }
    }

    if (mouse) {
        if (GetKeyState(VK_LBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON1);     }
        if (GetKeyState(VK_RBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON2);     }
        if (GetKeyState(VK_MBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON3);     }
        if (GetKeyState(VK_XBUTTON1) < 0) { set_modifier_mask(MASK_BUTTON4);     }
        if (GetKeyState(VK_XBUTTON2) < 0) { set_modifier_mask(MASK_BUTTON5);     }
    }
}

void unregister_running_hooks() {
    // Destroy the native hooks.
    if (keyboard_event_hhook != NULL) {
        UnhookWindowsHookEx(keyboard_event_hhook);
        keyboard_event_hhook = NULL;
    }

    if (mouse_event_hhook != NULL) {
        UnhookWindowsHookEx(mouse_event_hhook);
        mouse_event_hhook = NULL;
    }
}

LRESULT CALLBACK keyboard_hook_event_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    bool consumed = false;

    KBDLLHOOKSTRUCT *kbhook = (KBDLLHOOKSTRUCT *) lParam;
    switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            consumed = dispatch_key_press(kbhook);
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            consumed = dispatch_key_release(kbhook);
            break;

        default:
            // In theory this *should* never execute.
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Unhandled Windows keyboard event: %#X.\n",
                    __FUNCTION__, __LINE__, (unsigned int) wParam);
            break;
    }

    LRESULT hook_result = -1;
    if (nCode < 0 || !consumed) {
        hook_result = CallNextHookEx(keyboard_event_hhook, nCode, wParam, lParam);
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Consuming the current event. (%li)\n",
                __FUNCTION__, __LINE__, (long) hook_result);
    }

    return hook_result;
}

LRESULT CALLBACK mouse_hook_event_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    bool consumed = false;

    MSLLHOOKSTRUCT *mshook = (MSLLHOOKSTRUCT *) lParam;
    switch (wParam) {
        case WM_LBUTTONDOWN:
            set_modifier_mask(MASK_BUTTON1);
            consumed = dispatch_button_press(mshook, MOUSE_BUTTON1);
            break;

        case WM_RBUTTONDOWN:
            set_modifier_mask(MASK_BUTTON2);
            consumed = dispatch_button_press(mshook, MOUSE_BUTTON2);
            break;

        case WM_MBUTTONDOWN:
            set_modifier_mask(MASK_BUTTON3);
            consumed = dispatch_button_press(mshook, MOUSE_BUTTON3);
            break;

        case WM_XBUTTONDOWN:
        case WM_NCXBUTTONDOWN:
            if (HIWORD(mshook->mouseData) == XBUTTON1) {
                set_modifier_mask(MASK_BUTTON4);
                consumed = dispatch_button_press(mshook, MOUSE_BUTTON4);
            } else if (HIWORD(mshook->mouseData) == XBUTTON2) {
                set_modifier_mask(MASK_BUTTON5);
                consumed = dispatch_button_press(mshook, MOUSE_BUTTON5);
            } else {
                // Extra mouse buttons.
                uint16_t button = HIWORD(mshook->mouseData);

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    set_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    set_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_press(mshook, button);
            }
            break;


        case WM_LBUTTONUP:
            unset_modifier_mask(MASK_BUTTON1);
            consumed = dispatch_button_release(mshook, MOUSE_BUTTON1);
            break;

        case WM_RBUTTONUP:
            unset_modifier_mask(MASK_BUTTON2);
            consumed = dispatch_button_release(mshook, MOUSE_BUTTON2);
            break;

        case WM_MBUTTONUP:
            unset_modifier_mask(MASK_BUTTON3);
            consumed = dispatch_button_release(mshook, MOUSE_BUTTON3);
            break;

        case WM_XBUTTONUP:
        case WM_NCXBUTTONUP:
            if (HIWORD(mshook->mouseData) == XBUTTON1) {
                unset_modifier_mask(MASK_BUTTON4);
                consumed = dispatch_button_release(mshook, MOUSE_BUTTON4);
            } else if (HIWORD(mshook->mouseData) == XBUTTON2) {
                unset_modifier_mask(MASK_BUTTON5);
                consumed = dispatch_button_release(mshook, MOUSE_BUTTON5);
            } else {
                // Extra mouse buttons.
                uint16_t button = HIWORD(mshook->mouseData);

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    unset_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    unset_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_release(mshook, MOUSE_BUTTON5);
            }
            break;

        case WM_MOUSEMOVE:
            consumed = dispatch_mouse_move(mshook);
            break;

        case WM_MOUSEWHEEL:
            consumed = dispatch_mouse_wheel(mshook, WHEEL_VERTICAL_DIRECTION);
            break;

        case WM_MOUSEHWHEEL:
            consumed = dispatch_mouse_wheel(mshook, WHEEL_HORIZONTAL_DIRECTION);
            break;

        default:
            // In theory this *should* never execute.
            logger(LOG_LEVEL_WARN, "%s [%u]: Unhandled Windows mouse event: %#X.\n",
                    __FUNCTION__, __LINE__, (unsigned int) wParam);
            break;
    }

    LRESULT hook_result = -1;
    if (nCode < 0 || !consumed) {
        hook_result = CallNextHookEx(mouse_event_hhook, nCode, wParam, lParam);
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Consuming the current event. (%li)\n",
                __FUNCTION__, __LINE__, (long) hook_result);
    }

    return hook_result;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_DISPLAYCHANGE:
            enumerate_displays();
            break;
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static int create_invisible_window()
{
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = WS_EX_NOACTIVATE;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInst;
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "libuiohook";
    wcex.hIconSm = NULL;

    if (!invisible_win_class_initialized) {
        if (!RegisterClassEx(&wcex)) {
            DWORD error = GetLastError();

            if (error == 1410) { // Class already exists
                logger(LOG_LEVEL_WARN, "%s [%u]: RegisterClassEx: class already exists\n",
                    __FUNCTION__, __LINE__);
            }
            else {
                logger(LOG_LEVEL_ERROR, "%s [%u]: RegisterClassEx failed! (%#lX)\n",
                    __FUNCTION__, __LINE__, (unsigned long) error);
                return 0;
            }
        }
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Not calling RegisterClassEx; class already exists\n",
            __FUNCTION__, __LINE__);
    }

    invisible_win_class_initialized = TRUE;

    invisible_win_hwnd = CreateWindowEx(
            WS_EX_NOACTIVATE,
            "libuiohook",
            "Hidden Window to Monitor Display Change Events",
            WS_DISABLED,
            0,
            0,
            1,
            1,
            NULL,
            NULL,
            hInst,
            NULL
    );

    if (!invisible_win_hwnd) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CreateWindowEx failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long) GetLastError());
        return 0;
    }

    ShowWindow(invisible_win_hwnd, SW_HIDE);

    return 1;
}

int run(bool run_keyboard_hook, bool run_mouse_hook) {
    int status = UIOHOOK_FAILURE;

    // Set the thread id we want to signal later.
    hook_thread_id = GetCurrentThreadId();

    // Spot check the hInst in case the library was statically linked and DllMain
    // did not receive a pointer on load.
    if (hInst == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: hInst was not set by DllMain().\n",
                __FUNCTION__, __LINE__);

        hInst = GetModuleHandle(NULL);
        if (hInst == NULL) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Could not determine hInst for SetWindowsHookEx()! (%#lX)\n",
                    __FUNCTION__, __LINE__, (unsigned long) GetLastError());

            return UIOHOOK_ERROR_GET_MODULE_HANDLE;
        }
    }

    // Create invisible window to receive monitor change events
    if (!create_invisible_window() || invisible_win_hwnd == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Create invisible window failed! (%#lX)\n",
               __FUNCTION__, __LINE__, (unsigned long) GetLastError());

        return UIOHOOK_ERROR_CREATE_INVISIBLE_WINDOW;
    }

    // Create the native hooks.

    if (run_keyboard_hook) {
        keyboard_event_hhook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_event_proc, hInst, 0);
    }

    if (run_mouse_hook) {
        mouse_event_hhook = SetWindowsHookEx(WH_MOUSE_LL, mouse_hook_event_proc, hInst, 0);
    }

    // If we did not encounter a problem, start processing events.
    if ((!run_keyboard_hook || keyboard_event_hhook != NULL) && (!run_mouse_hook || mouse_event_hhook != NULL)) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: SetWindowsHookEx() successful.\n",
                __FUNCTION__, __LINE__);

        // Check and setup modifiers.
        initialize_modifiers(run_keyboard_hook, run_mouse_hook);

        // Set the exit status.
        status = UIOHOOK_SUCCESS;

        // Windows does not have a hook start event or callback so we need to
        // manually fake it.
        dispatch_hook_enable();

        // Block until the thread receives an WM_QUIT request.
        MSG message;
        while (GetMessage(&message, (HWND) NULL, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: SetWindowsHookEx() failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long) GetLastError());

        status = UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX;
    }

    // Unregister any hooks that may still be installed.
    unregister_running_hooks();

    // We must explicitly call the cleanup handler because Windows does not
    // provide a thread cleanup method like POSIX pthread_cleanup_push/pop.
    dispatch_hook_disable();

    return status;
}

UIOHOOK_API int hook_run() {
    return run(true, true);
}

UIOHOOK_API int hook_run_keyboard() {
    return run(true, false);
}

UIOHOOK_API int hook_run_mouse() {
    return run(false, true);
}

UIOHOOK_API int hook_stop() {
    int status = UIOHOOK_FAILURE;

    // Destroy the invisible window
    if (PostMessage(invisible_win_hwnd, WM_CLOSE, 0, 0)) {
        // Try to exit the thread naturally.
        if (PostThreadMessage(hook_thread_id, WM_QUIT, (WPARAM) NULL, (LPARAM) NULL)) {
            status = UIOHOOK_SUCCESS;
        }
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}
