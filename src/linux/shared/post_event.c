#include <stdbool.h>
#include <stdint.h>

#include <linux/input-event-codes.h>

#include <logger.h>
#include <uiohook.h>

#include "backend.h"
#include "input_helper.h"
#include "uinput_helper.h"

#define WHEEL_DELTA 120

#define WHEEL_AXIS_VERTICAL   0
#define WHEEL_AXIS_HORIZONTAL 1

static int32_t wheel_remainder[2] = { 0, 0 };

static int post_key_event(uiohook_event * const event) {
    uint16_t evdev_code = uiocode_to_evdev_code(event->data.keyboard.keycode);
    if (evdev_code == 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to look up the evdev key code: %u\n",
                __FUNCTION__, __LINE__, event->data.keyboard.keycode);

        return UIOHOOK_FAILURE;
    }

    return post_virtual_key(evdev_code, event->type == EVENT_KEY_PRESSED);
}

static int32_t normalize_position(int16_t position, uint16_t size) {
    if (position < 0) {
        position = 0;
    } else if (position >= size) {
        position = size - 1;
    }

    return (int32_t) ((position + 0.5) * ABSOLUTE_AXIS_MAX / size);
}

static int post_mouse_motion_absolute(int16_t x, int16_t y) {
    uint16_t width, height;
    if (!backend_get_desktop_bounds(&width, &height) || width == 0 || height == 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Cannot post an absolute position as the desktop bounds "
                "are unavailable!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }

    backend_restore_absolute_position(&x, &y);

    virtual_event events[] = {
        { .type = EV_ABS, .code = ABS_X, .value = normalize_position(x, width) },
        { .type = EV_ABS, .code = ABS_Y, .value = normalize_position(y, height) }
    };

    return post_virtual_events(VIRTUAL_DEVICE_POINTER, events, sizeof(events) / sizeof(events[0]));
}

static int post_mouse_motion_event(uiohook_event * const event) {
    if (event->type == EVENT_MOUSE_MOVED_RELATIVE || event->type == EVENT_MOUSE_DRAGGED_RELATIVE) {
        // The display server applies its pointer acceleration profile to relative motion, so the
        // pointer will not necessarily move by the exact number of pixels which was posted.
        virtual_event events[] = {
            { .type = EV_REL, .code = REL_X, .value = event->data.mouse.x },
            { .type = EV_REL, .code = REL_Y, .value = event->data.mouse.y }
        };

        return post_virtual_events(VIRTUAL_DEVICE_POINTER, events, sizeof(events) / sizeof(events[0]));
    }

    return post_mouse_motion_absolute(event->data.mouse.x, event->data.mouse.y);
}

static int post_mouse_button_event(uiohook_event * const event) {
    uint16_t evdev_code = button_to_evdev_code(event->data.mouse.button);
    if (evdev_code == 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Invalid button specified for a mouse button event: %u\n",
                __FUNCTION__, __LINE__, event->data.mouse.button);

        return UIOHOOK_FAILURE;
    }

    bool pressed = event->type == EVENT_MOUSE_PRESSED || event->type == EVENT_MOUSE_PRESSED_IGNORE_COORDS;
    bool ignore_coords = event->type == EVENT_MOUSE_PRESSED_IGNORE_COORDS
            || event->type == EVENT_MOUSE_RELEASED_IGNORE_COORDS;

    if (!ignore_coords) {
        // Move the pointer to the specified position first.
        int status = post_mouse_motion_absolute(event->data.mouse.x, event->data.mouse.y);
        if (status != UIOHOOK_SUCCESS) {
            return status;
        }
    }

    virtual_event events[] = {
        { .type = EV_KEY, .code = evdev_code, .value = pressed ? 1 : 0 }
    };

    return post_virtual_events(VIRTUAL_DEVICE_POINTER, events, sizeof(events) / sizeof(events[0]));
}

static int post_mouse_wheel_event(uiohook_event * const event) {
    bool horizontal = event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION;

    // uiohook reports positive values for scrolling up and left, and evdev - for up and right.
    int32_t value = horizontal ? -event->data.wheel.rotation : event->data.wheel.rotation;
    if (value == 0) {
        return UIOHOOK_SUCCESS;
    }

    unsigned int axis = horizontal ? WHEEL_AXIS_HORIZONTAL : WHEEL_AXIS_VERTICAL;
    int32_t total = wheel_remainder[axis] + value;
    wheel_remainder[axis] = total % WHEEL_DELTA;

    // Events whose value is zero are dropped by the kernel, so a fraction of a click is posted only on
    // the high-resolution axis.
    virtual_event events[] = {
        { .type = EV_REL, .code = horizontal ? REL_HWHEEL_HI_RES : REL_WHEEL_HI_RES, .value = value },
        { .type = EV_REL, .code = horizontal ? REL_HWHEEL : REL_WHEEL, .value = total / WHEEL_DELTA }
    };

    return post_virtual_events(VIRTUAL_DEVICE_POINTER, events, sizeof(events) / sizeof(events[0]));
}

int hook_init_virtual_devices(const char * const application_name) {
    return create_virtual_devices(application_name);
}

int hook_destroy_virtual_devices() {
    return destroy_virtual_devices();
}

int hook_post_event(uiohook_event * const event) {
    return hook_post_events(event, 1);
}

int hook_post_events(uiohook_event * const events, uint32_t size) {
    if (events == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Not simulating any events as the events are null.\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_NULL;
    }

    if (size == 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Not simulating any events as the size is 0.\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_SUCCESS;
    }

    int status = lock_virtual_devices();
    if (status != UIOHOOK_SUCCESS) {
        return status;
    }

    for (uint32_t i = 0; i < size && status == UIOHOOK_SUCCESS; i++) {
        uiohook_event *event = events + i;

        switch (event->type) {
            case EVENT_KEY_PRESSED:
            case EVENT_KEY_RELEASED:
                status = post_key_event(event);
                break;

            case EVENT_MOUSE_PRESSED:
            case EVENT_MOUSE_PRESSED_IGNORE_COORDS:
            case EVENT_MOUSE_RELEASED:
            case EVENT_MOUSE_RELEASED_IGNORE_COORDS:
                status = post_mouse_button_event(event);
                break;

            case EVENT_MOUSE_MOVED:
            case EVENT_MOUSE_MOVED_RELATIVE:
            case EVENT_MOUSE_DRAGGED:
            case EVENT_MOUSE_DRAGGED_RELATIVE:
                status = post_mouse_motion_event(event);
                break;

            case EVENT_MOUSE_WHEEL:
                status = post_mouse_wheel_event(event);
                break;

            case EVENT_KEY_TYPED:
            case EVENT_MOUSE_CLICKED:

            case EVENT_HOOK_ENABLED:
            case EVENT_HOOK_DISABLED:

            default:
                logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring post event type %#X\n",
                        __FUNCTION__, __LINE__, event->type);

                status = UIOHOOK_FAILURE;
                break;
        }
    }

    unlock_virtual_devices();

    return status;
}
