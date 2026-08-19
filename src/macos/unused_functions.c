// Functions in this file do nothing since they are specific to other platforms

#include <uiohook.h>

// Linux-specific functions

uint64_t hook_get_post_text_delay_linux() {
    return 0;
}

void hook_set_post_text_delay_linux(uint64_t delay) {
}

int hook_get_linux_mode() {
    return LINUX_MODE_AUTO_XRECORD;
}

int hook_set_linux_mode(int mode) {
    return UIOHOOK_SUCCESS;
}

int hook_get_loaded_linux_backend() {
    return LINUX_LOADED_BACKEND_NONE;
}

int hook_init_virtual_devices(const char * const application_name) {
    return UIOHOOK_SUCCESS;
}

int hook_destroy_virtual_devices() {
    return UIOHOOK_SUCCESS;
}

void hook_set_device_procs(device_open_t open_proc, device_close_t close_proc, void *user_data) {
}
