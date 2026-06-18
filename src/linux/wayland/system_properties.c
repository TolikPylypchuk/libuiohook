#include <logger.h>
#include <uiohook.h>

screen_data* hook_create_screen_info(unsigned char *count) {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_create_screen_info is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    if (count != NULL) {
        *count = 0;
    }

    return NULL;
}

long int hook_get_auto_repeat_rate() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_get_auto_repeat_rate is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_auto_repeat_delay() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_get_auto_repeat_delay is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_pointer_acceleration_multiplier() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_get_pointer_acceleration_multiplier is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_pointer_acceleration_threshold() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_get_pointer_acceleration_threshold is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_pointer_sensitivity() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_get_pointer_sensitivity is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_multi_click_time() {
    logger(LOG_LEVEL_ERROR, "%s [%u]: hook_get_multi_click_time is not implemented for Wayland yet.\n",
            __FUNCTION__, __LINE__);

    return -1;
}
