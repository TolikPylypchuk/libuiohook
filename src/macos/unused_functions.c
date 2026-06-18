// Functions in this file do nothing since they are specific to other platforms

#include <uiohook.h>

// Linux-specific functions

uint64_t hook_get_post_text_delay_x11() {
    return 0;
}

void hook_set_post_text_delay_x11(uint64_t delay) {
}

int hook_get_linux_backend() {
    return LINUX_BACKEND_AUTO;
}

bool hook_set_linux_backend(int backend) {
    return true;
}
