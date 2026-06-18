#include <logger.h>
#include <uiohook.h>

int hook_post_event(uiohook_event * const event) {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_post_event is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_FAILURE;
}

int hook_post_events(uiohook_event * const events, uint32_t size) {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_post_events is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_FAILURE;
}

int hook_post_text(const uint16_t * const text) {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_post_text is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_FAILURE;
}

uint64_t hook_get_post_text_delay_x11() {
    return 0;
}

void hook_set_post_text_delay_x11(uint64_t delay) {
}
