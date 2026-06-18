#include <logger.h>
#include <uiohook.h>

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
