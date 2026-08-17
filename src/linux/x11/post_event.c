#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <uiohook.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xkbcommon/xkbcommon.h>

#include "input_helper.h"
#include "logger.h"
#include "uinput_helper.h"

#define BORROWED_KEYCODES_MAX 32
#define KEYSYM_SHIFT_LEVELS 4

static uint64_t post_text_delay = 50 * 1000000;

uint64_t hook_get_post_text_delay_x11() {
    return post_text_delay;
}

void hook_set_post_text_delay_x11(uint64_t delay) {
    post_text_delay = delay;
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
        keysyms[i] = xkb_utf32_to_keysym(utf32_text[i]);

        if (keysyms[i] == NoSymbol) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Could not map character %04X to a key sym!\n",
                __FUNCTION__, __LINE__, utf32_text[i]);
        }
    }

    *keysym_count = i;
    free(utf32_text);

    return keysyms;
}

static size_t find_unused_keycodes(KeyCode *keycodes, size_t length) {
    int min_keycode = 0, max_keycode = 0;
    if (!XDisplayKeycodes(helper_disp, &min_keycode, &max_keycode)) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XDisplayKeycodes() failed!\n",
                __FUNCTION__, __LINE__);
        return 0;
    }

    if (min_keycode <= EVDEV_KEYCODE_OFFSET) {
        min_keycode = EVDEV_KEYCODE_OFFSET + 1;
    }

    int keysyms_per_keycode = 0;
    KeySym *keysyms = XGetKeyboardMapping(
            helper_disp, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

    if (keysyms == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XGetKeyboardMapping() failed!\n",
                __FUNCTION__, __LINE__);
        return 0;
    }

    size_t count = 0;

    for (int keycode = max_keycode; keycode >= min_keycode && count < length; keycode--) {
        bool used = false;

        for (int i = 0; i < keysyms_per_keycode && !used; i++) {
            used = keysyms[(keycode - min_keycode) * keysyms_per_keycode + i] != NoSymbol;
        }

        if (!used) {
            keycodes[count++] = (KeyCode) keycode;
        }
    }

    XFree(keysyms);

    return count;
}

static int map_keysym(KeyCode keycode, KeySym keysym) {
    KeySym keysyms[KEYSYM_SHIFT_LEVELS] = { keysym, keysym, keysym, keysym };
    int result = XChangeKeyboardMapping(helper_disp, keycode, KEYSYM_SHIFT_LEVELS, keysyms, 1);

    if (result != Success) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XChangeKeyboardMapping() failed! (%d)\n",
                __FUNCTION__, __LINE__, result);
        return UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int unmap_keysym(KeyCode keycode) {
    KeySym keysyms[KEYSYM_SHIFT_LEVELS] = { NoSymbol, NoSymbol, NoSymbol, NoSymbol };
    int result = XChangeKeyboardMapping(helper_disp, keycode, KEYSYM_SHIFT_LEVELS, keysyms, 1);

    if (result != Success) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XChangeKeyboardMapping() failed! (%d)\n",
                __FUNCTION__, __LINE__, result);
        return UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int press_keycode(KeyCode keycode) {
    uint16_t evdev_code = keycode - EVDEV_KEYCODE_OFFSET;

    int status = post_virtual_key(evdev_code, true);
    if (status == UIOHOOK_SUCCESS) {
        status = post_virtual_key(evdev_code, false);
    }

    return status;
}

static void wait_for_delay() {
    struct timespec ts = {
        .tv_sec = post_text_delay / 1000000000,
        .tv_nsec = post_text_delay % 1000000000
    };

    nanosleep(&ts, NULL);
}

int hook_post_text(const uint16_t * const text) {
    if (text == NULL) {
        return UIOHOOK_ERROR_NULL;
    }

    if (helper_disp == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    int lock_status = lock_virtual_devices();
    if (lock_status != UIOHOOK_SUCCESS) {
        return lock_status;
    }

    XLockDisplay(helper_disp);

    size_t count = 0;

    for (int i = 0; text[i] != 0; i++) {
        count++;
    }

    KeyCode keycodes[BORROWED_KEYCODES_MAX];
    size_t keycode_count = find_unused_keycodes(keycodes, BORROWED_KEYCODES_MAX);

    if (keycode_count == 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Cannot find an unused key code!\n",
                __FUNCTION__, __LINE__);

        XUnlockDisplay(helper_disp);
        unlock_virtual_devices();
        return UIOHOOK_FAILURE;
    }

    size_t keysym_count = 0;
    KeySym *keysyms = map_to_keysyms(text, count, &keysym_count);

    KeyCode *press_keycodes = calloc(keysym_count, sizeof(KeyCode));

    int status = keysyms != NULL && (press_keycodes != NULL || keysym_count == 0)
        ? UIOHOOK_SUCCESS
        : UIOHOOK_ERROR_OUT_OF_MEMORY;

    for (size_t index = 0; index < keysym_count && status == UIOHOOK_SUCCESS; ) {
        size_t mapped = 0;
        size_t end = index;

        while (end < keysym_count && status == UIOHOOK_SUCCESS) {
            if (keysyms[end] == NoSymbol) {
                end++;
                continue;
            }

            KeyCode keycode = 0;
            for (size_t i = index; i < end && keycode == 0; i++) {
                if (keysyms[i] == keysyms[end]) {
                    keycode = press_keycodes[i];
                }
            }

            if (keycode == 0) {
                if (mapped == keycode_count) {
                    break;
                }

                keycode = keycodes[mapped++];
                status = map_keysym(keycode, keysyms[end]);
            }

            press_keycodes[end++] = keycode;
        }

        if (status != UIOHOOK_SUCCESS) {
            break;
        }

        XSync(helper_disp, True);
        wait_for_delay();

        for (size_t i = index; i < end && status == UIOHOOK_SUCCESS; i++) {
            if (press_keycodes[i] != 0) {
                status = press_keycode(press_keycodes[i]);
                wait_for_delay();
            }
        }

        wait_for_delay();

        index = end;
    }

    free(press_keycodes);
    free(keysyms);

    wait_for_delay();

    for (size_t i = 0; i < keycode_count; i++) {
        if (unmap_keysym(keycodes[i]) != UIOHOOK_SUCCESS) {
            status = UIOHOOK_FAILURE;
        }
    }

    XSync(helper_disp, True);
    XUnlockDisplay(helper_disp);
    unlock_virtual_devices();

    return status;
}
