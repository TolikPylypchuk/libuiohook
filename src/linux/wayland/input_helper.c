#include <uiohook.h>

int hook_get_linux_backend() {
    return LINUX_BACKEND_WAYLAND;
}

bool hook_set_linux_backend(int backend) {
    return false;
}
