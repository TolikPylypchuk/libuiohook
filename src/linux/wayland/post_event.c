#include <logger.h>
#include <uiohook.h>

int hook_post_text(const uint16_t * const text) {
    logger(LOG_LEVEL_WARN, "%s [%u]: hook_post_text is not supported on Wayland.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_ERROR_UNSUPPORTED_FEATURE;
}

uint64_t hook_get_post_text_delay_x11() {
    return 0;
}

void hook_set_post_text_delay_x11(uint64_t delay) {
}
