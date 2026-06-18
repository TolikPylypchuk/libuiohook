#include <uiohook.h>

int hook_get_linux_backend() {
    return LINUX_BACKEND_WAYLAND;
}

bool hook_set_linux_backend(int backend) {
    return false;
}

bool hook_is_ax_api_enabled(bool promptUserIfDisabled) {
    return true;
}

bool hook_get_prompt_user_if_ax_api_disabled() {
    return false;
}

void hook_set_prompt_user_if_ax_api_disabled(bool promptUserIfDisabled) {
}

uint32_t hook_get_ax_poll_frequency() {
    return 0;
}

void hook_set_ax_poll_frequency(uint32_t frequency) {
}
