#include <stdint.h>
#include <stdio.h>

#include <linux/input-event-codes.h>

#include "minunit.h"
#include "shared/input_helper.h"
#include "uiohook.h"

static char * test_evdev_code_round_trip() {
    printf("Testing the evdev key code round trip.\n");

    for (uint16_t evdev_code = 0; evdev_code <= KEY_MAX; evdev_code++) {
        uint16_t uiocode = evdev_code_to_uiocode(evdev_code);

        if (uiocode == VC_UNDEFINED) {
            continue;
        }

        mu_assert("error, the evdev key code round trip produced a different key code",
                uiocode_to_evdev_code(uiocode) == evdev_code);
    }

    return NULL;
}

static char * test_uiocode_round_trip() {
    printf("Testing the uiohook key code round trip.\n");

    for (uint32_t uiocode = 0; uiocode <= UINT16_MAX; uiocode++) {
        uint16_t evdev_code = uiocode_to_evdev_code((uint16_t) uiocode);

        if (evdev_code == 0) {
            continue;
        }

        mu_assert("error, the uiohook key code round trip produced a different key code",
                evdev_code_to_uiocode(evdev_code) == uiocode);
    }

    return NULL;
}

static char * test_key_code_mappings() {
    printf("Testing several known key code mappings.\n");

    mu_assert("error, VC_ESCAPE is not mapped to KEY_ESC", uiocode_to_evdev_code(VC_ESCAPE) == KEY_ESC);
    mu_assert("error, VC_A is not mapped to KEY_A", uiocode_to_evdev_code(VC_A) == KEY_A);
    mu_assert("error, VC_1 is not mapped to KEY_1", uiocode_to_evdev_code(VC_1) == KEY_1);
    mu_assert("error, VC_SHIFT_R is not mapped to KEY_RIGHTSHIFT", uiocode_to_evdev_code(VC_SHIFT_R) == KEY_RIGHTSHIFT);
    mu_assert("error, VC_KP_5 is not mapped to KEY_KP5", uiocode_to_evdev_code(VC_KP_5) == KEY_KP5);
    mu_assert("error, VC_MEDIA_PLAY is not mapped to KEY_PLAYPAUSE",
            uiocode_to_evdev_code(VC_MEDIA_PLAY) == KEY_PLAYPAUSE);
    mu_assert("error, VC_BROWSER_BACK is not mapped to KEY_BACK",
            uiocode_to_evdev_code(VC_BROWSER_BACK) == KEY_BACK);

    mu_assert("error, KEY_RESERVED is mapped to a key code", evdev_code_to_uiocode(KEY_RESERVED) == VC_UNDEFINED);
    mu_assert("error, an unmapped key code is not undefined", evdev_code_to_uiocode(KEY_MAX) == VC_UNDEFINED);
    mu_assert("error, an unmapped uiohook key code is mapped", uiocode_to_evdev_code(VC_UNDEFINED) == 0);

    return NULL;
}

static char * test_button_mappings() {
    printf("Testing the mouse button mappings.\n");

    mu_assert("error, MOUSE_BUTTON1 is not mapped to BTN_LEFT", button_to_evdev_code(MOUSE_BUTTON1) == BTN_LEFT);
    mu_assert("error, MOUSE_BUTTON2 is not mapped to BTN_RIGHT", button_to_evdev_code(MOUSE_BUTTON2) == BTN_RIGHT);
    mu_assert("error, MOUSE_BUTTON3 is not mapped to BTN_MIDDLE", button_to_evdev_code(MOUSE_BUTTON3) == BTN_MIDDLE);
    mu_assert("error, MOUSE_BUTTON4 is not mapped to BTN_SIDE", button_to_evdev_code(MOUSE_BUTTON4) == BTN_SIDE);
    mu_assert("error, MOUSE_BUTTON5 is not mapped to BTN_EXTRA", button_to_evdev_code(MOUSE_BUTTON5) == BTN_EXTRA);

    for (uint16_t button = MOUSE_BUTTON1; button <= MOUSE_BUTTON5; button++) {
        mu_assert("error, the mouse button round trip produced a different button",
                evdev_code_to_button(button_to_evdev_code(button)) == button);
    }

    mu_assert("error, MOUSE_NOBUTTON is mapped to a button code", button_to_evdev_code(MOUSE_NOBUTTON) == 0);
    mu_assert("error, a key code is mapped to a mouse button", evdev_code_to_button(KEY_A) == MOUSE_NOBUTTON);

    return NULL;
}

static char * test_modifier_masks() {
    printf("Testing the modifier masks.\n");

    mu_assert("error, VC_SHIFT_L has no modifier mask", get_modifier_mask_for_uiocode(VC_SHIFT_L) == MASK_SHIFT_L);
    mu_assert("error, VC_CONTROL_R has no modifier mask", get_modifier_mask_for_uiocode(VC_CONTROL_R) == MASK_CTRL_R);
    mu_assert("error, VC_META_L has no modifier mask", get_modifier_mask_for_uiocode(VC_META_L) == MASK_META_L);
    mu_assert("error, VC_A has a modifier mask", get_modifier_mask_for_uiocode(VC_A) == 0);

    mu_assert("error, VC_CAPS_LOCK has no lock mask", get_lock_mask_for_uiocode(VC_CAPS_LOCK) == MASK_CAPS_LOCK);
    mu_assert("error, VC_NUM_LOCK has no lock mask", get_lock_mask_for_uiocode(VC_NUM_LOCK) == MASK_NUM_LOCK);
    mu_assert("error, VC_SHIFT_L has a lock mask", get_lock_mask_for_uiocode(VC_SHIFT_L) == 0);

    mu_assert("error, MOUSE_BUTTON1 has no modifier mask", get_modifier_mask_for_button(MOUSE_BUTTON1) == MASK_BUTTON1);
    mu_assert("error, MOUSE_NOBUTTON has a modifier mask", get_modifier_mask_for_button(MOUSE_NOBUTTON) == 0);

    clear_modifier_mask();
    mu_assert("error, the modifier mask is not empty", get_modifiers() == 0);

    set_modifier_mask(MASK_SHIFT_L);
    set_modifier_mask(MASK_CAPS_LOCK);
    mu_assert("error, the modifier mask was not set", get_modifiers() == (MASK_SHIFT_L | MASK_CAPS_LOCK));

    unset_modifier_mask(MASK_SHIFT_L);
    mu_assert("error, the modifier mask was not unset", get_modifiers() == MASK_CAPS_LOCK);

    clear_modifier_mask();
    mu_assert("error, the modifier mask was not cleared", get_modifiers() == 0);

    return NULL;
}

char * evdev_input_helper_tests() {
    mu_run_test(test_evdev_code_round_trip);
    mu_run_test(test_uiocode_round_trip);
    mu_run_test(test_key_code_mappings);
    mu_run_test(test_button_mappings);
    mu_run_test(test_modifier_masks);

    return NULL;
}
