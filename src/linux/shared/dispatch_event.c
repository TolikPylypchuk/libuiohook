#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include <sys/time.h>

#include <libinput.h>
#include <linux/input-event-codes.h>

#include <logger.h>
#include <uiohook.h>

#include "backend.h"
#include "dispatch_event.h"
#include "input_helper.h"

// libinput reports 120 units per wheel click, which is the same unit as WHEEL_DELTA on Windows.
#define WHEEL_DELTA                 120
#define WHEEL_SCROLL_LINES          3

// Finger and continuous scrolling are reported in pixels instead of wheel clicks.
#define SCROLL_PIXELS_PER_CLICK     10.0

typedef struct _mouse_click {
    uint16_t count;
    uint64_t time;
    uint16_t button;
} mouse_click;

static mouse_click click = {
    .count = 0,
    .time = 0,
    .button = MOUSE_NOBUTTON
};

// Whether the pointer moved between the last press and release, which suppresses the click event.
static bool pointer_moved = false;

static bool desktop_bounds_unavailable_logged = false;

static uiohook_event uio_event;

static dispatcher_t dispatch = NULL;
static void *dispatch_data = NULL;

static bool key_typed_enabled = false;

static bool is_key_typed_supported() {
    return hook_get_optional_feature_support() & UIOHOOK_FEATURE_KEY_TYPED_EVENTS;
}

bool hook_is_key_typed_enabled() {
    return key_typed_enabled && is_key_typed_supported();
}

void hook_set_key_typed_enabled(bool enabled) {
    if (enabled && !is_key_typed_supported()) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Key typed events are not supported on this back-end.\n",
                __FUNCTION__, __LINE__);

        return;
    }

    key_typed_enabled = enabled;
}

void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting new dispatch callback to %#p.\n",
            __FUNCTION__, __LINE__, dispatch_proc);

    dispatch = dispatch_proc;
    dispatch_data = user_data;
}

static uint64_t get_unix_timestamp() {
    struct timeval system_time;

    gettimeofday(&system_time, NULL);

    return (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);
}

static int16_t round_to_int16(double value) {
    return (int16_t) (value + (value >= 0 ? 0.5 : -0.5));
}

static uint64_t get_multi_click_time() {
    long int multi_click_time = hook_get_multi_click_time();
    return multi_click_time > 0 ? (uint64_t) multi_click_time : 0;
}

static void dispatch_event(uiohook_event *const uio_event) {
    if (dispatch != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Dispatching event type %u.\n",
                __FUNCTION__, __LINE__, uio_event->type);

        dispatch(uio_event, dispatch_data);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: No dispatch callback set!\n",
                __FUNCTION__, __LINE__);
    }
}

static void get_pointer_position(int16_t *x, int16_t *y) {
    if (!backend_get_pointer_position(x, y)) {
        *x = 0;
        *y = 0;
    }
}

void dispatch_hook_enabled() {
    click.count = 0;
    click.time = 0;
    click.button = MOUSE_NOBUTTON;
    pointer_moved = false;
    desktop_bounds_unavailable_logged = false;

    uio_event.time = get_unix_timestamp();
    uio_event.type = EVENT_HOOK_ENABLED;
    uio_event.mask = 0x00;

    dispatch_event(&uio_event);
}

void dispatch_hook_disabled() {
    uio_event.time = get_unix_timestamp();
    uio_event.type = EVENT_HOOK_DISABLED;
    uio_event.mask = 0x00;

    dispatch_event(&uio_event);
}

static void dispatch_key_typed(uint64_t timestamp, uint16_t evdev_code, uint16_t uiocode, bool emulated) {
    wchar_t surrogate[2] = {};
    size_t count = backend_key_to_unicode(evdev_code, get_modifiers(), surrogate, sizeof(surrogate) / sizeof(wchar_t));

    for (size_t i = 0; i < count; i++) {
        uio_event.time = timestamp;
        uio_event.type = EVENT_KEY_TYPED;
        uio_event.mask = get_modifiers();
        if (emulated) {
            uio_event.mask |= MASK_EMULATED;
        }

        uio_event.data.keyboard.keycode = uiocode;
        uio_event.data.keyboard.rawcode = evdev_code;
        uio_event.data.keyboard.keychar = surrogate[i];

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X typed. (%lc)\n",
                __FUNCTION__, __LINE__,
                uio_event.data.keyboard.keycode, uio_event.data.keyboard.keychar);

        dispatch_event(&uio_event);
    }
}

static void dispatch_key(uint64_t timestamp, struct libinput_event_keyboard *keyboard_event, bool emulated) {
    uint16_t evdev_code = (uint16_t) libinput_event_keyboard_get_key(keyboard_event);
    uint16_t uiocode = evdev_code_to_uiocode(evdev_code);
    bool pressed = libinput_event_keyboard_get_key_state(keyboard_event) == LIBINPUT_KEY_STATE_PRESSED;

    uint16_t mask = get_modifier_mask_for_uiocode(uiocode);
    uint16_t lock_mask = get_lock_mask_for_uiocode(uiocode);

    if (mask != 0) {
        if (pressed) {
            set_modifier_mask(mask);
        } else {
            unset_modifier_mask(mask);
        }
    } else if (lock_mask != 0 && pressed) {
        // The lock masks follow the LEDs, which toggle on press and don't change on release.
        if (get_modifiers() & lock_mask) {
            unset_modifier_mask(lock_mask);
        } else {
            set_modifier_mask(lock_mask);
        }
    }

    uio_event.time = timestamp;
    uio_event.type = pressed ? EVENT_KEY_PRESSED : EVENT_KEY_RELEASED;
    uio_event.mask = get_modifiers();
    if (emulated) {
        uio_event.mask |= MASK_EMULATED;
    }

    uio_event.data.keyboard.keycode = uiocode;
    uio_event.data.keyboard.rawcode = evdev_code;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X %s. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, pressed ? "pressed" : "released",
            uio_event.data.keyboard.rawcode);

    dispatch_event(&uio_event);

    if (pressed && hook_is_key_typed_enabled()) {
        dispatch_key_typed(timestamp, evdev_code, uiocode, emulated);
    }
}

static void dispatch_mouse_clicked(uint64_t timestamp, uint16_t button, bool emulated) {
    uio_event.time = timestamp;
    uio_event.type = EVENT_MOUSE_CLICKED;
    uio_event.mask = get_modifiers();
    if (emulated) {
        uio_event.mask |= MASK_EMULATED;
    }

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click.count;
    get_pointer_position(&uio_event.data.mouse.x, &uio_event.data.mouse.y);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    dispatch_event(&uio_event);
}

static void dispatch_mouse_button(uint64_t timestamp, struct libinput_event_pointer *pointer_event, bool emulated) {
    uint16_t button = evdev_code_to_button((uint16_t) libinput_event_pointer_get_button(pointer_event));
    if (button == MOUSE_NOBUTTON) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Ignoring unmapped button %u.\n",
                __FUNCTION__, __LINE__, libinput_event_pointer_get_button(pointer_event));

        return;
    }

    bool pressed = libinput_event_pointer_get_button_state(pointer_event) == LIBINPUT_BUTTON_STATE_PRESSED;
    uint16_t mask = get_modifier_mask_for_button(button);

    if (pressed) {
        set_modifier_mask(mask);

        // Track the number of clicks, the button must match the previous button.
        if (button == click.button && timestamp - click.time <= get_multi_click_time()) {
            if (click.count < UINT16_MAX) {
                click.count++;
            } else {
                logger(LOG_LEVEL_WARN, "%s [%u]: Click count overflow detected!\n",
                        __FUNCTION__, __LINE__);
            }
        } else {
            click.count = 1;
            click.button = button;
        }

        click.time = timestamp;
        pointer_moved = false;
    } else {
        unset_modifier_mask(mask);
    }

    uio_event.time = timestamp;
    uio_event.type = pressed ? EVENT_MOUSE_PRESSED : EVENT_MOUSE_RELEASED;
    uio_event.mask = get_modifiers();
    if (emulated) {
        uio_event.mask |= MASK_EMULATED;
    }

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click.count;
    get_pointer_position(&uio_event.data.mouse.x, &uio_event.data.mouse.y);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u %s %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, pressed ? "pressed" : "released", uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    dispatch_event(&uio_event);

    if (!pressed && !pointer_moved) {
        dispatch_mouse_clicked(timestamp, button, emulated);
    }
}

static void dispatch_mouse_moved(uint64_t timestamp, int16_t x, int16_t y, bool absolute, bool emulated) {
    pointer_moved = true;

    if (click.count != 0 && timestamp - click.time > get_multi_click_time()) {
        click.count = 0;
    }

    bool button_held = get_modifiers() & (MASK_BUTTON1 | MASK_BUTTON2 | MASK_BUTTON3 | MASK_BUTTON4 | MASK_BUTTON5);

    uio_event.time = timestamp;
    uio_event.mask = get_modifiers();
    if (emulated) {
        uio_event.mask |= MASK_EMULATED;
    }

    if (absolute) {
        uio_event.type = button_held ? EVENT_MOUSE_DRAGGED : EVENT_MOUSE_MOVED;
    } else {
        uio_event.type = button_held ? EVENT_MOUSE_DRAGGED_RELATIVE : EVENT_MOUSE_MOVED_RELATIVE;
    }

    uio_event.data.mouse.button = MOUSE_NOBUTTON;
    uio_event.data.mouse.clicks = click.count;
    uio_event.data.mouse.x = x;
    uio_event.data.mouse.y = y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse %s to %i, %i. (%#X)\n",
            __FUNCTION__, __LINE__,
            button_held ? "dragged" : "moved",
            uio_event.data.mouse.x, uio_event.data.mouse.y, uio_event.mask);

    dispatch_event(&uio_event);
}

static void dispatch_mouse_motion(uint64_t timestamp, struct libinput_event_pointer *pointer_event, bool emulated) {
    int16_t x, y;

    if (backend_get_pointer_position(&x, &y)) {
        dispatch_mouse_moved(timestamp, x, y, true, emulated);
    } else {
        dispatch_mouse_moved(
            timestamp,
            round_to_int16(libinput_event_pointer_get_dx(pointer_event)),
            round_to_int16(libinput_event_pointer_get_dy(pointer_event)),
            false,
            emulated);
    }
}

static void dispatch_mouse_motion_absolute(uint64_t timestamp, struct libinput_event_pointer *pointer_event, bool emulated) {
    int16_t x, y;
    uint16_t width, height;

    if (backend_get_desktop_bounds(&width, &height)) {
        x = round_to_int16(libinput_event_pointer_get_absolute_x_transformed(pointer_event, width));
        y = round_to_int16(libinput_event_pointer_get_absolute_y_transformed(pointer_event, height));
    } else if (!backend_get_pointer_position(&x, &y)) {
        if (!desktop_bounds_unavailable_logged) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring absolute motion as the desktop bounds are unavailable!\n",
                    __FUNCTION__, __LINE__);

            desktop_bounds_unavailable_logged = true;
        }

        return;
    }

    dispatch_mouse_moved(timestamp, x, y, true, emulated);
}

static void dispatch_mouse_wheel(
        uint64_t timestamp,
        struct libinput_event_pointer *pointer_event,
        enum libinput_pointer_axis axis,
        bool wheel,
        bool emulated) {
    double value = wheel
        ? libinput_event_pointer_get_scroll_value_v120(pointer_event, axis)
        : libinput_event_pointer_get_scroll_value(pointer_event, axis) * WHEEL_DELTA / SCROLL_PIXELS_PER_CLICK;

    if (value == 0) {
        return;
    }

    bool vertical = axis == LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;

    click.count = 0;
    click.button = MOUSE_NOBUTTON;

    uio_event.time = timestamp;
    uio_event.type = EVENT_MOUSE_WHEEL;
    uio_event.mask = get_modifiers();
    if (emulated) {
        uio_event.mask |= MASK_EMULATED;
    }

    get_pointer_position(&uio_event.data.wheel.x, &uio_event.data.wheel.y);

    uio_event.data.wheel.type = WHEEL_UNIT_SCROLL;
    uio_event.data.wheel.delta = WHEEL_DELTA;
    uio_event.data.wheel.direction = vertical ? WHEEL_VERTICAL_DIRECTION : WHEEL_HORIZONTAL_DIRECTION;

    // libinput reports positive values for scrolling down and right.
    uio_event.data.wheel.rotation = -round_to_int16(value * WHEEL_SCROLL_LINES);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse wheel %i / %u of type %u in the %u direction at %u, %u.\n",
            __FUNCTION__, __LINE__,
            uio_event.data.wheel.rotation, uio_event.data.wheel.delta,
            uio_event.data.wheel.type, uio_event.data.wheel.direction,
            uio_event.data.wheel.x, uio_event.data.wheel.y);

    dispatch_event(&uio_event);
}

void dispatch_libinput_event(struct libinput_event *event, bool emulated) {
    uint64_t timestamp = get_unix_timestamp();

    switch (libinput_event_get_type(event)) {
        case LIBINPUT_EVENT_KEYBOARD_KEY:
            dispatch_key(timestamp, libinput_event_get_keyboard_event(event), emulated);
            break;

        case LIBINPUT_EVENT_POINTER_MOTION:
            dispatch_mouse_motion(timestamp, libinput_event_get_pointer_event(event), emulated);
            break;

        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
            dispatch_mouse_motion_absolute(timestamp, libinput_event_get_pointer_event(event), emulated);
            break;

        case LIBINPUT_EVENT_POINTER_BUTTON:
            dispatch_mouse_button(timestamp, libinput_event_get_pointer_event(event), emulated);
            break;

        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
        case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
        case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
            struct libinput_event_pointer *pointer_event = libinput_event_get_pointer_event(event);
            bool wheel = libinput_event_get_type(event) == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL;

            if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
                dispatch_mouse_wheel(
                        timestamp, pointer_event,
                        LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL, wheel, emulated);
            }

            if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
                dispatch_mouse_wheel(
                        timestamp, pointer_event,
                        LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL, wheel, emulated);
            }
            break;

        default:
            break;
    }
}
