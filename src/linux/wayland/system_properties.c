#include <uiohook.h>

screen_data* hook_create_screen_info(unsigned char *count) {
    if (count != NULL) {
        *count = 0;
    }
    return NULL;
}

long int hook_get_auto_repeat_rate() {
    return -1;
}

long int hook_get_auto_repeat_delay() {
    return -1;
}

long int hook_get_pointer_acceleration_multiplier() {
    return -1;
}

long int hook_get_pointer_acceleration_threshold() {
    return -1;
}

long int hook_get_pointer_sensitivity() {
    return -1;
}

long int hook_get_multi_click_time() {
    return -1;
}
