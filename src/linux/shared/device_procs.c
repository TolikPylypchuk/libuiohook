#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <uiohook.h>

#include "device_procs.h"

static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;

static device_procs procs = {
    .open_proc = NULL,
    .close_proc = NULL,
    .user_data = NULL
};

void hook_set_device_procs(device_open_t open_proc, device_close_t close_proc, void *user_data) {
    pthread_mutex_lock(&device_mutex);

    procs.open_proc = open_proc;
    procs.close_proc = close_proc;
    procs.user_data = user_data;

    pthread_mutex_unlock(&device_mutex);
}

device_procs get_device_procs() {
    pthread_mutex_lock(&device_mutex);

    device_procs current = procs;

    pthread_mutex_unlock(&device_mutex);

    return current;
}

int open_device(const device_procs *procs, const char *path, int flags) {
    if (procs->open_proc != NULL) {
        return procs->open_proc(path, flags, procs->user_data);
    }

    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

void close_device(const device_procs *procs, int fd) {
    if (procs->close_proc != NULL) {
        procs->close_proc(fd, procs->user_data);
        return;
    }

    close(fd);
}
