#ifndef DEVICE_PROCS_H
#define DEVICE_PROCS_H

#include <uiohook.h>

typedef struct _device_procs {
    device_open_t open_proc;
    device_close_t close_proc;
    void *user_data;
} device_procs;

/* Gets the procs which are currently set. */
device_procs get_device_procs();

/* Opens a device node through the given procs, falling back to open(). */
int open_device(const device_procs *procs, const char *path, int flags);

/* Closes a descriptor which was returned by open_device. */
void close_device(const device_procs *procs, int fd);

#endif
