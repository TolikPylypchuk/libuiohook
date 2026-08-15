#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include <logger.h>
#include <uiohook.h>

#include "backend.h"

int hook_run() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_run is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_FAILURE;
}

int hook_run_keyboard() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_run_keyboard is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_FAILURE;
}

int hook_run_mouse() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_run_mouse is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_FAILURE;
}

int hook_stop() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_stop is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_FAILURE;
}

size_t backend_key_to_unicode(uint16_t evdev_code, uint16_t modifier_mask, wchar_t *buffer, size_t length) {
    return 0;
}

bool backend_get_pointer_position(int16_t *x, int16_t *y) {
    // Not supported by Wayland
    return false;
}

bool backend_get_desktop_bounds(uint16_t *width, uint16_t *height) {
    return false;
}
