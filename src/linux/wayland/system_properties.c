#include <logger.h>
#include <uiohook.h>

#include "monitor_helper.h"
#include "wayland_helper.h"

uint32_t hook_get_optional_feature_support() {
    return 0;
}

screen_data* hook_create_screen_info(unsigned char *count) {
    if (!wayland_helper_init()) {
        *count = 0;
        return NULL;
    }

    return monitor_helper_create_screen_info(count);
}

long int hook_get_auto_repeat_rate() {
    int32_t rate = wayland_helper_get_repeat_rate();
    return rate > 0 ? 1000 / rate : -1;
}

long int hook_get_auto_repeat_delay() {
    int32_t delay = wayland_helper_get_repeat_delay();
    return delay >= 0 ? delay : -1;
}

long int hook_get_pointer_acceleration_multiplier() {
    logger(LOG_LEVEL_WARN, "%s [%u]: The pointer acceleration multiplier is not available on Wayland.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_pointer_acceleration_threshold() {
    logger(LOG_LEVEL_WARN, "%s [%u]: The pointer acceleration threshold is not available on Wayland.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_pointer_sensitivity() {
    logger(LOG_LEVEL_WARN, "%s [%u]: The pointer sensitivity is not available on Wayland.\n",
            __FUNCTION__, __LINE__);

    return -1;
}

long int hook_get_multi_click_time() {
    // Not supported by Wayland, so return the default value for GNOME, KDE, and GTK.
    return 400;
}
