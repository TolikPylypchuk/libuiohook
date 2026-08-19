#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/eventfd.h>

#include <libinput.h>
#include <libudev.h>

#include <logger.h>
#include <uiohook.h>

#include "device_procs.h"
#include "dispatch_event.h"
#include "input_helper.h"
#include "input_loop.h"

#define VIRTUAL_DEVICE_PATH         "/sys/devices/virtual/"

static int stop_fd = -1;

static pthread_mutex_t stop_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

static device_procs procs;

static const char emulated_device;

static unsigned int input_device_count = 0;

static int open_restricted(const char *path, int flags, void *user_data) {
    return open_device(&procs, path, flags);
}

static void close_restricted(int fd, void *user_data) {
    close_device(&procs, fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted
};

static void add_device(struct libinput_device *device) {
    if (!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD)
            && !libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER)) {
        return;
    }

    input_device_count++;

    // Tapping is disabled by default in libinput, but most desktop environments enable it, so a tap
    // on a touchpad must be reported as a button event here as well.
    if (libinput_device_config_tap_get_finger_count(device) > 0
            && libinput_device_config_tap_set_enabled(device, LIBINPUT_CONFIG_TAP_ENABLED)
                != LIBINPUT_CONFIG_STATUS_SUCCESS) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to enable tapping for %s!\n",
                __FUNCTION__, __LINE__, libinput_device_get_name(device));
    }

    struct udev_device *udev_device = libinput_device_get_udev_device(device);
    if (udev_device == NULL) {
        return;
    }

    const char *syspath = udev_device_get_syspath(udev_device);
    if (syspath != NULL && strncmp(syspath, VIRTUAL_DEVICE_PATH, strlen(VIRTUAL_DEVICE_PATH)) == 0) {
        libinput_device_set_user_data(device, (void *) &emulated_device);
    }

    const char *devnode = udev_device_get_devnode(udev_device);
    if (devnode != NULL) {
        int fd = open_device(&procs, devnode, O_RDONLY | O_NONBLOCK);

        if (fd >= 0) {
            seed_modifier_mask(fd);
            close_device(&procs, fd);
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to open %s to read the modifier state: %s\n",
                    __FUNCTION__, __LINE__, devnode, strerrorname_np(-fd));
        }
    }

    udev_device_unref(udev_device);
}

static void remove_device(struct libinput_device *device) {
    if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD)
            || libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER)) {
        input_device_count--;
    }
}

static bool is_device_event(enum libinput_event_type event_type) {
    return event_type == LIBINPUT_EVENT_DEVICE_ADDED || event_type == LIBINPUT_EVENT_DEVICE_REMOVED;
}

static void handle_event(struct libinput_event *event, bool keyboard, bool mouse) {
    struct libinput_device *device = libinput_event_get_device(event);
    bool emulated = libinput_device_get_user_data(device) != NULL;

    switch (libinput_event_get_type(event)) {
        case LIBINPUT_EVENT_DEVICE_ADDED:
            add_device(device);
            break;

        case LIBINPUT_EVENT_DEVICE_REMOVED:
            remove_device(device);
            break;

        case LIBINPUT_EVENT_KEYBOARD_KEY:
            if (keyboard) {
                dispatch_libinput_event(event, emulated);
            }
            break;

        case LIBINPUT_EVENT_POINTER_MOTION:
        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
        case LIBINPUT_EVENT_POINTER_BUTTON:
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
        case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
        case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
            if (mouse) {
                dispatch_libinput_event(event, emulated);
            }
            break;

        default:
            break;
    }
}

static struct libinput_event *count_devices(struct libinput *li) {
    if (libinput_dispatch(li) != 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to dispatch libinput events!\n",
                __FUNCTION__, __LINE__);

        return NULL;
    }

    struct libinput_event *event;

    while ((event = libinput_get_event(li)) != NULL) {
        if (!is_device_event(libinput_event_get_type(event))) {
            return event;
        }

        handle_event(event, false, false);
        libinput_event_destroy(event);
    }

    return NULL;
}

static void handle_events(struct libinput *li, bool keyboard, bool mouse) {
    if (libinput_dispatch(li) != 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to dispatch libinput events!\n",
                __FUNCTION__, __LINE__);

        return;
    }

    struct libinput_event *event;

    while ((event = libinput_get_event(li)) != NULL) {
        handle_event(event, keyboard, mouse);
        libinput_event_destroy(event);
    }
}

int run_libinput(bool keyboard, bool mouse) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Creating a udev context.\n",
            __FUNCTION__, __LINE__);

    struct udev *udev = udev_new();

    if (udev == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create a udev context!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_LINUX_INIT_UDEV;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Creating a libinput context.\n",
            __FUNCTION__, __LINE__);

    procs = get_device_procs();

    struct libinput *li = libinput_udev_create_context(&interface, procs.user_data, udev);

    if (li == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create a libinput context!\n",
                __FUNCTION__, __LINE__);

        udev_unref(udev);
        return UIOHOOK_ERROR_LINUX_INIT_LIBINPUT;
    }

    const char *seat = getenv("XDG_SEAT");
    if (seat == NULL || seat[0] == '\0') {
        seat = "seat0";
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Assigning the libinput context to %s.\n",
            __FUNCTION__, __LINE__, seat);

    int error = libinput_udev_assign_seat(li, seat);
    if (error) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Libinput seat assignment has failed! (%#X)\n",
                __FUNCTION__, __LINE__, error);

        libinput_unref(li);
        udev_unref(udev);
        return UIOHOOK_ERROR_LINUX_ASSIGN_SEAT;
    }

    struct pollfd fds[2];

    fds[0].fd = libinput_get_fd(li);
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    pthread_mutex_lock(&stop_fd_mutex);
    stop_fd = eventfd(0, EFD_NONBLOCK);
    pthread_mutex_unlock(&stop_fd_mutex);

    if (stop_fd < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create a stop notification file descriptor: %s\n",
                __FUNCTION__, __LINE__, strerrorname_np(errno));

        libinput_unref(li);
        udev_unref(udev);
        return UIOHOOK_ERROR_LINUX_INIT_STOP_NOTIFICATION;
    }

    fds[1].fd = stop_fd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    clear_modifier_mask();
    input_device_count = 0;

    struct libinput_event *pending_event = count_devices(li);

    int status = UIOHOOK_SUCCESS;

    if (input_device_count == 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: No keyboard or pointer devices are available! "
                "Access to /dev/input is most likely missing.\n",
                __FUNCTION__, __LINE__);

        status = UIOHOOK_ERROR_LINUX_NO_INPUT_DEVICES;

        if (pending_event != NULL) {
            libinput_event_destroy(pending_event);
        }
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Watching %u input device(s).\n",
                __FUNCTION__, __LINE__, input_device_count);

        dispatch_hook_enabled();

        if (pending_event != NULL) {
            handle_event(pending_event, keyboard, mouse);
            libinput_event_destroy(pending_event);
        }

        handle_events(li, keyboard, mouse);

        bool running = true;
        while (running) {
            int result = poll(fds, 2, -1);
            if (result < 0) {
                if (errno == EINTR) { // We don't care about interruptions here.
                    continue;
                }

                logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to poll for events: %s\n",
                        __FUNCTION__, __LINE__, strerrorname_np(errno));

                break;
            }

            if (fds[0].revents & POLLIN) {
                handle_events(li, keyboard, mouse);
            }

            if (fds[1].revents & POLLIN) {
                running = false;
            }
        }

        dispatch_hook_disabled();
    }

    pthread_mutex_lock(&stop_fd_mutex);
    close(stop_fd);
    stop_fd = -1;
    pthread_mutex_unlock(&stop_fd_mutex);

    libinput_unref(li);
    udev_unref(udev);

    return status;
}

int stop_libinput() {
    pthread_mutex_lock(&stop_fd_mutex);

    if (stop_fd < 0) {
        pthread_mutex_unlock(&stop_fd_mutex);
        return UIOHOOK_SUCCESS;
    }

    uint64_t value = 1;
    if (write(stop_fd, &value, sizeof(value)) < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to write to the stop notification file descriptor: %s\n",
                __FUNCTION__, __LINE__, strerrorname_np(errno));

        pthread_mutex_unlock(&stop_fd_mutex);
        return UIOHOOK_ERROR_LINUX_EXEC_STOP_NOTIFICATION;
    }

    pthread_mutex_unlock(&stop_fd_mutex);
    return UIOHOOK_SUCCESS;
}
