#include <stdint.h>
#include <string.h>

#include <linux/input.h>
#include <sys/ioctl.h>

#include <uiohook.h>

#include "input_helper.h"

#define BITS_PER_LONG           (sizeof(long) * 8)
#define NBITS(x)                (((x) - 1) / BITS_PER_LONG + 1)
#define TEST_BIT(bits, bit)     ((bits)[(bit) / BITS_PER_LONG] >> ((bit) % BITS_PER_LONG) & 1)

typedef struct _key_mapping {
    uint16_t uiocode;
    uint16_t evdev_code;
} key_mapping;

static uint16_t modifier_mask;

static const key_mapping uiocode_evdev_table[] = {
    { .uiocode = VC_ESCAPE,            .evdev_code = KEY_ESC },
    { .uiocode = VC_F1,                .evdev_code = KEY_F1 },
    { .uiocode = VC_F2,                .evdev_code = KEY_F2 },
    { .uiocode = VC_F3,                .evdev_code = KEY_F3 },
    { .uiocode = VC_F4,                .evdev_code = KEY_F4 },
    { .uiocode = VC_F5,                .evdev_code = KEY_F5 },
    { .uiocode = VC_F6,                .evdev_code = KEY_F6 },
    { .uiocode = VC_F7,                .evdev_code = KEY_F7 },
    { .uiocode = VC_F8,                .evdev_code = KEY_F8 },
    { .uiocode = VC_F9,                .evdev_code = KEY_F9 },
    { .uiocode = VC_F10,               .evdev_code = KEY_F10 },
    { .uiocode = VC_F11,               .evdev_code = KEY_F11 },
    { .uiocode = VC_F12,               .evdev_code = KEY_F12 },
    { .uiocode = VC_F13,               .evdev_code = KEY_F13 },
    { .uiocode = VC_F14,               .evdev_code = KEY_F14 },
    { .uiocode = VC_F15,               .evdev_code = KEY_F15 },
    { .uiocode = VC_F16,               .evdev_code = KEY_F16 },
    { .uiocode = VC_F17,               .evdev_code = KEY_F17 },
    { .uiocode = VC_F18,               .evdev_code = KEY_F18 },
    { .uiocode = VC_F19,               .evdev_code = KEY_F19 },
    { .uiocode = VC_F20,               .evdev_code = KEY_F20 },
    { .uiocode = VC_F21,               .evdev_code = KEY_F21 },
    { .uiocode = VC_F22,               .evdev_code = KEY_F22 },
    { .uiocode = VC_F23,               .evdev_code = KEY_F23 },
    { .uiocode = VC_F24,               .evdev_code = KEY_F24 },
    { .uiocode = VC_BACK_QUOTE,        .evdev_code = KEY_GRAVE },
    { .uiocode = VC_1,                 .evdev_code = KEY_1 },
    { .uiocode = VC_2,                 .evdev_code = KEY_2 },
    { .uiocode = VC_3,                 .evdev_code = KEY_3 },
    { .uiocode = VC_4,                 .evdev_code = KEY_4 },
    { .uiocode = VC_5,                 .evdev_code = KEY_5 },
    { .uiocode = VC_6,                 .evdev_code = KEY_6 },
    { .uiocode = VC_7,                 .evdev_code = KEY_7 },
    { .uiocode = VC_8,                 .evdev_code = KEY_8 },
    { .uiocode = VC_9,                 .evdev_code = KEY_9 },
    { .uiocode = VC_0,                 .evdev_code = KEY_0 },
    { .uiocode = VC_MINUS,             .evdev_code = KEY_MINUS },
    { .uiocode = VC_EQUALS,            .evdev_code = KEY_EQUAL },
    { .uiocode = VC_BACKSPACE,         .evdev_code = KEY_BACKSPACE },
    { .uiocode = VC_TAB,               .evdev_code = KEY_TAB },
    { .uiocode = VC_Q,                 .evdev_code = KEY_Q },
    { .uiocode = VC_W,                 .evdev_code = KEY_W },
    { .uiocode = VC_E,                 .evdev_code = KEY_E },
    { .uiocode = VC_R,                 .evdev_code = KEY_R },
    { .uiocode = VC_T,                 .evdev_code = KEY_T },
    { .uiocode = VC_Y,                 .evdev_code = KEY_Y },
    { .uiocode = VC_U,                 .evdev_code = KEY_U },
    { .uiocode = VC_I,                 .evdev_code = KEY_I },
    { .uiocode = VC_O,                 .evdev_code = KEY_O },
    { .uiocode = VC_P,                 .evdev_code = KEY_P },
    { .uiocode = VC_OPEN_BRACKET,      .evdev_code = KEY_LEFTBRACE },
    { .uiocode = VC_CLOSE_BRACKET,     .evdev_code = KEY_RIGHTBRACE },
    { .uiocode = VC_ENTER,             .evdev_code = KEY_ENTER },
    { .uiocode = VC_CAPS_LOCK,         .evdev_code = KEY_CAPSLOCK },
    { .uiocode = VC_A,                 .evdev_code = KEY_A },
    { .uiocode = VC_S,                 .evdev_code = KEY_S },
    { .uiocode = VC_D,                 .evdev_code = KEY_D },
    { .uiocode = VC_F,                 .evdev_code = KEY_F },
    { .uiocode = VC_G,                 .evdev_code = KEY_G },
    { .uiocode = VC_H,                 .evdev_code = KEY_H },
    { .uiocode = VC_J,                 .evdev_code = KEY_J },
    { .uiocode = VC_K,                 .evdev_code = KEY_K },
    { .uiocode = VC_L,                 .evdev_code = KEY_L },
    { .uiocode = VC_SEMICOLON,         .evdev_code = KEY_SEMICOLON },
    { .uiocode = VC_QUOTE,             .evdev_code = KEY_APOSTROPHE },
    { .uiocode = VC_BACK_SLASH,        .evdev_code = KEY_BACKSLASH },
    { .uiocode = VC_SHIFT_L,           .evdev_code = KEY_LEFTSHIFT },
    { .uiocode = VC_Z,                 .evdev_code = KEY_Z },
    { .uiocode = VC_X,                 .evdev_code = KEY_X },
    { .uiocode = VC_C,                 .evdev_code = KEY_C },
    { .uiocode = VC_V,                 .evdev_code = KEY_V },
    { .uiocode = VC_B,                 .evdev_code = KEY_B },
    { .uiocode = VC_N,                 .evdev_code = KEY_N },
    { .uiocode = VC_M,                 .evdev_code = KEY_M },
    { .uiocode = VC_COMMA,             .evdev_code = KEY_COMMA },
    { .uiocode = VC_PERIOD,            .evdev_code = KEY_DOT },
    { .uiocode = VC_SLASH,             .evdev_code = KEY_SLASH },
    { .uiocode = VC_SHIFT_R,           .evdev_code = KEY_RIGHTSHIFT },
    { .uiocode = VC_102,               .evdev_code = KEY_102ND },
    { .uiocode = VC_ALT_L,             .evdev_code = KEY_LEFTALT },
    { .uiocode = VC_CONTROL_L,         .evdev_code = KEY_LEFTCTRL },
    { .uiocode = VC_META_L,            .evdev_code = KEY_LEFTMETA },
    { .uiocode = VC_SPACE,             .evdev_code = KEY_SPACE },
    { .uiocode = VC_META_R,            .evdev_code = KEY_RIGHTMETA },
    { .uiocode = VC_CONTROL_R,         .evdev_code = KEY_RIGHTCTRL },
    { .uiocode = VC_ALT_R,             .evdev_code = KEY_RIGHTALT },
    { .uiocode = VC_CONTEXT_MENU,      .evdev_code = KEY_COMPOSE },
    { .uiocode = VC_PRINT_SCREEN,      .evdev_code = KEY_SYSRQ },
    { .uiocode = VC_SCROLL_LOCK,       .evdev_code = KEY_SCROLLLOCK },
    { .uiocode = VC_PAUSE,             .evdev_code = KEY_PAUSE },
    { .uiocode = VC_INSERT,            .evdev_code = KEY_INSERT },
    { .uiocode = VC_HOME,              .evdev_code = KEY_HOME },
    { .uiocode = VC_PAGE_UP,           .evdev_code = KEY_PAGEUP },
    { .uiocode = VC_DELETE,            .evdev_code = KEY_DELETE },
    { .uiocode = VC_END,               .evdev_code = KEY_END },
    { .uiocode = VC_PAGE_DOWN,         .evdev_code = KEY_PAGEDOWN },
    { .uiocode = VC_UP,                .evdev_code = KEY_UP },
    { .uiocode = VC_LEFT,              .evdev_code = KEY_LEFT },
    { .uiocode = VC_DOWN,              .evdev_code = KEY_DOWN },
    { .uiocode = VC_RIGHT,             .evdev_code = KEY_RIGHT },
    { .uiocode = VC_NUM_LOCK,          .evdev_code = KEY_NUMLOCK },
    { .uiocode = VC_KP_DIVIDE,         .evdev_code = KEY_KPSLASH },
    { .uiocode = VC_KP_MULTIPLY,       .evdev_code = KEY_KPASTERISK },
    { .uiocode = VC_KP_SUBTRACT,       .evdev_code = KEY_KPMINUS },
    { .uiocode = VC_KP_7,              .evdev_code = KEY_KP7 },
    { .uiocode = VC_KP_8,              .evdev_code = KEY_KP8 },
    { .uiocode = VC_KP_9,              .evdev_code = KEY_KP9 },
    { .uiocode = VC_KP_ADD,            .evdev_code = KEY_KPPLUS },
    { .uiocode = VC_KP_4,              .evdev_code = KEY_KP4 },
    { .uiocode = VC_KP_5,              .evdev_code = KEY_KP5 },
    { .uiocode = VC_KP_6,              .evdev_code = KEY_KP6 },
    { .uiocode = VC_KP_1,              .evdev_code = KEY_KP1 },
    { .uiocode = VC_KP_2,              .evdev_code = KEY_KP2 },
    { .uiocode = VC_KP_3,              .evdev_code = KEY_KP3 },
    { .uiocode = VC_KP_ENTER,          .evdev_code = KEY_KPENTER },
    { .uiocode = VC_KP_0,              .evdev_code = KEY_KP0 },
    { .uiocode = VC_KP_DECIMAL,        .evdev_code = KEY_KPDOT },
    { .uiocode = VC_KP_EQUALS,         .evdev_code = KEY_KPEQUAL },
    { .uiocode = VC_KATAKANA_HIRAGANA, .evdev_code = KEY_KATAKANAHIRAGANA },
    { .uiocode = VC_UNDERSCORE,        .evdev_code = KEY_RO },
    { .uiocode = VC_CONVERT,           .evdev_code = KEY_HENKAN },
    { .uiocode = VC_NONCONVERT,        .evdev_code = KEY_MUHENKAN },
    { .uiocode = VC_YEN,               .evdev_code = KEY_YEN },
    { .uiocode = VC_KATAKANA,          .evdev_code = KEY_KATAKANA },
    { .uiocode = VC_HIRAGANA,          .evdev_code = KEY_HIRAGANA },
    { .uiocode = VC_JP_COMMA,          .evdev_code = KEY_KPJPCOMMA },
    { .uiocode = VC_KANA,              .evdev_code = KEY_HANGEUL },
    { .uiocode = VC_HANJA,             .evdev_code = KEY_HANJA },
    { .uiocode = VC_VOLUME_MUTE,       .evdev_code = KEY_MUTE },
    { .uiocode = VC_VOLUME_DOWN,       .evdev_code = KEY_VOLUMEDOWN },
    { .uiocode = VC_VOLUME_UP,         .evdev_code = KEY_VOLUMEUP },
    { .uiocode = VC_POWER,             .evdev_code = KEY_POWER },
    { .uiocode = VC_HELP,              .evdev_code = KEY_HELP },
    { .uiocode = VC_KP_SEPARATOR,      .evdev_code = KEY_KPCOMMA },
    { .uiocode = VC_APP_CALCULATOR,    .evdev_code = KEY_CALC },
    { .uiocode = VC_SLEEP,             .evdev_code = KEY_SLEEP },
    { .uiocode = VC_MODE_CHANGE,       .evdev_code = KEY_XFER },
    { .uiocode = VC_APP_1,             .evdev_code = KEY_PROG1 },
    { .uiocode = VC_APP_2,             .evdev_code = KEY_PROG2 },
    { .uiocode = VC_APP_BROWSER,       .evdev_code = KEY_WWW },
    { .uiocode = VC_APP_MAIL,          .evdev_code = KEY_MAIL },
    { .uiocode = VC_BROWSER_FAVORITES, .evdev_code = KEY_BOOKMARKS },
    { .uiocode = VC_BROWSER_BACK,      .evdev_code = KEY_BACK },
    { .uiocode = VC_BROWSER_FORWARD,   .evdev_code = KEY_FORWARD },
    { .uiocode = VC_MEDIA_EJECT,       .evdev_code = KEY_EJECTCD },
    { .uiocode = VC_MEDIA_NEXT,        .evdev_code = KEY_NEXTSONG },
    { .uiocode = VC_MEDIA_PLAY,        .evdev_code = KEY_PLAYPAUSE },
    { .uiocode = VC_MEDIA_PREVIOUS,    .evdev_code = KEY_PREVIOUSSONG },
    { .uiocode = VC_MEDIA_STOP,        .evdev_code = KEY_STOPCD },
    { .uiocode = VC_BROWSER_HOME,      .evdev_code = KEY_HOMEPAGE },
    { .uiocode = VC_BROWSER_REFRESH,   .evdev_code = KEY_REFRESH },
    { .uiocode = VC_APP_3,             .evdev_code = KEY_PROG3 },
    { .uiocode = VC_APP_4,             .evdev_code = KEY_PROG4 },
    { .uiocode = VC_BROWSER_SEARCH,    .evdev_code = KEY_SEARCH },
    { .uiocode = VC_CANCEL,            .evdev_code = KEY_CANCEL },
    { .uiocode = VC_BROWSER_STOP,      .evdev_code = KEY_STOP },
    { .uiocode = VC_MEDIA_SELECT,      .evdev_code = KEY_MEDIA },
};

#define BUTTON_MAX  (MOUSE_BUTTON1 + BTN_TASK - BTN_LEFT)

uint16_t evdev_code_to_uiocode(uint16_t evdev_code) {
    for (unsigned int i = 0; i < sizeof(uiocode_evdev_table) / sizeof(uiocode_evdev_table[0]); i++) {
        if (evdev_code == uiocode_evdev_table[i].evdev_code) {
            return uiocode_evdev_table[i].uiocode;
        }
    }

    return VC_UNDEFINED;
}

uint16_t uiocode_to_evdev_code(uint16_t uiocode) {
    for (unsigned int i = 0; i < sizeof(uiocode_evdev_table) / sizeof(uiocode_evdev_table[0]); i++) {
        if (uiocode == uiocode_evdev_table[i].uiocode) {
            return uiocode_evdev_table[i].evdev_code;
        }
    }

    return 0;
}

uint16_t evdev_code_to_button(uint16_t evdev_code) {
    if (evdev_code < BTN_LEFT || evdev_code > BTN_TASK) {
        return MOUSE_NOBUTTON;
    }

    return evdev_code - BTN_LEFT + MOUSE_BUTTON1;
}

uint16_t button_to_evdev_code(uint16_t button) {
    if (button < MOUSE_BUTTON1 || button > BUTTON_MAX) {
        return 0;
    }

    return button - MOUSE_BUTTON1 + BTN_LEFT;
}

uint16_t get_modifier_mask_for_uiocode(uint16_t uiocode) {
    switch (uiocode) {
        case VC_SHIFT_L:
            return MASK_SHIFT_L;
        case VC_SHIFT_R:
            return MASK_SHIFT_R;
        case VC_CONTROL_L:
            return MASK_CTRL_L;
        case VC_CONTROL_R:
            return MASK_CTRL_R;
        case VC_ALT_L:
            return MASK_ALT_L;
        case VC_ALT_R:
            return MASK_ALT_R;
        case VC_META_L:
            return MASK_META_L;
        case VC_META_R:
            return MASK_META_R;
        default:
            return 0;
    }
}

uint16_t get_lock_mask_for_uiocode(uint16_t uiocode) {
    switch (uiocode) {
        case VC_CAPS_LOCK:
            return MASK_CAPS_LOCK;
        case VC_NUM_LOCK:
            return MASK_NUM_LOCK;
        case VC_SCROLL_LOCK:
            return MASK_SCROLL_LOCK;
        default:
            return 0;
    }
}

uint16_t get_modifier_mask_for_button(uint16_t button) {
    switch (button) {
        case MOUSE_BUTTON1:
            return MASK_BUTTON1;
        case MOUSE_BUTTON2:
            return MASK_BUTTON2;
        case MOUSE_BUTTON3:
            return MASK_BUTTON3;
        case MOUSE_BUTTON4:
            return MASK_BUTTON4;
        case MOUSE_BUTTON5:
            return MASK_BUTTON5;
        default:
            return 0;
    }
}

void set_modifier_mask(uint16_t mask) {
    modifier_mask |= mask;
}

void unset_modifier_mask(uint16_t mask) {
    modifier_mask &= ~mask;
}

void clear_modifier_mask() {
    modifier_mask = 0;
}

uint16_t get_modifiers() {
    return modifier_mask;
}

void seed_modifier_mask(int fd) {
    unsigned long keys[NBITS(KEY_MAX)] = {};

    if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) >= 0) {
        for (unsigned int i = 0; i < sizeof(uiocode_evdev_table) / sizeof(uiocode_evdev_table[0]); i++) {
            uint16_t mask = get_modifier_mask_for_uiocode(uiocode_evdev_table[i].uiocode);
            if (mask != 0 && TEST_BIT(keys, uiocode_evdev_table[i].evdev_code)) {
                set_modifier_mask(mask);
            }
        }

        for (uint16_t button = MOUSE_BUTTON1; button <= MOUSE_BUTTON5; button++) {
            if (TEST_BIT(keys, button_to_evdev_code(button))) {
                set_modifier_mask(get_modifier_mask_for_button(button));
            }
        }
    }

    unsigned long leds[NBITS(LED_MAX)] = {};

    if (ioctl(fd, EVIOCGLED(sizeof(leds)), leds) >= 0) {
        if (TEST_BIT(leds, LED_CAPSL)) {
            set_modifier_mask(MASK_CAPS_LOCK);
        }

        if (TEST_BIT(leds, LED_NUML)) {
            set_modifier_mask(MASK_NUM_LOCK);
        }

        if (TEST_BIT(leds, LED_SCROLLL)) {
            set_modifier_mask(MASK_SCROLL_LOCK);
        }
    }
}
