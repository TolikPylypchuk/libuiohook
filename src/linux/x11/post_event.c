#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <uiohook.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <xkbcommon/xkbcommon.h>

#include "input_helper.h"
#include "logger.h"

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
    if (keysym == NoSymbol) {
        return UIOHOOK_SUCCESS;
    }

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

int hook_post_text(const uint16_t * const text) {
    if (text == NULL) {
        return UIOHOOK_ERROR_NULL;
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
