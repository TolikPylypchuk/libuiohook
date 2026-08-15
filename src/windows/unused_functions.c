// Functions in this file do nothing since they are specific to other platforms

#include <uiohook.h>

// macOS-specific functions

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

int hook_init_virtual_devices() {
    return UIOHOOK_SUCCESS;
}

int hook_destroy_virtual_devices() {
    return UIOHOOK_SUCCESS;
}

void hook_set_device_procs(device_open_t open_proc, device_close_t close_proc, void *user_data) {
}
