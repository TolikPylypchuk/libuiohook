#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include <uiohook.h>

#include "backend.h"
#include "input_loop.h"
#include "monitor_helper.h"
#include "wayland_helper.h"

size_t backend_key_to_unicode(uint16_t evdev_code, uint16_t modifier_mask, wchar_t *buffer, size_t length) {
    // Key typed events are not supported by this back-end.
    return 0;
}

bool backend_get_pointer_position(int16_t *x, int16_t *y) {
    // Wayland exposes no way to query the pointer position.
    return false;
}

bool backend_get_desktop_bounds(uint16_t *width, uint16_t *height) {
    return wayland_helper_init() && monitor_helper_get_desktop_bounds(width, height);
}

void backend_adjust_absolute_position(int16_t *x, int16_t *y) {
    // Absolute positions are already in the coordinate space which Wayland reports.
}

static int run(bool keyboard, bool mouse) {
    if (mouse) {
        wayland_helper_init();
    }

    return run_libinput(keyboard, mouse);
}

int hook_run() {
    return run(true, true);
}

int hook_run_keyboard() {
    return run(true, false);
}

int hook_run_mouse() {
    return run(false, true);
}

int hook_stop() {
    return stop_libinput();
}
