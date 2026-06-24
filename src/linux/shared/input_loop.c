#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/input-event-codes.h>
#include <sys/eventfd.h>

#include <libinput.h>
#include <libudev.h>

#include <logger.h>
#include <uiohook.h>

#include "input_loop.h"

static int stop_fd = -1;
static pthread_mutex_t stop_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

static int open_restricted(const char *path, int flags, void *user_data) {
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    close(fd);
}

const static struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted
};

static void handle_events(struct libinput *li) {
    libinput_dispatch(li);

    struct libinput_event *event;

    while ((event = libinput_get_event(li)) != NULL) {
        enum libinput_event_type event_type = libinput_event_get_type(event);

        if (event_type == LIBINPUT_EVENT_POINTER_AXIS) {
			libinput_event_destroy(event);
			continue;
		}

        switch (event_type) {
            case LIBINPUT_EVENT_KEYBOARD_KEY:
                struct libinput_event_keyboard *keyboard_event = libinput_event_get_keyboard_event(event);
                uint32_t key = libinput_event_keyboard_get_key(keyboard_event);
                enum libinput_key_state state = libinput_event_keyboard_get_key_state(keyboard_event);

                break;

            case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
                struct libinput_event_pointer *pointer_event = libinput_event_get_pointer_event(event);

                break;
        }

        libinput_event_destroy(event);
        libinput_dispatch(li);
    }
}

int run_libinput() {
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

    struct libinput *li = libinput_udev_create_context(&interface, NULL, udev);

    if (li == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create a libinput context!\n",
                __FUNCTION__, __LINE__);

        udev_unref(udev);
        return UIOHOOK_ERROR_LINUX_INIT_LIBINPUT;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Assigning the libinput context to seat0.\n",
            __FUNCTION__, __LINE__);

    int error = libinput_udev_assign_seat(li, "seat0");
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

    if (stop_fd < -1) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create a stop notification file descriptor: %s\n",
                __FUNCTION__, __LINE__, strerrorname_np(errno));

        return UIOHOOK_ERROR_LINUX_INIT_STOP_NOTIFICATION;
    }

    fds[1].fd = stop_fd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    handle_events(li);

    bool running = true;
    while (running) {
        int result = poll(fds, 2, -1) > -1;
        if (result < 0) {
            if (errno == EINTR) { // We don't care about interruptions here.
                continue;
            }

            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to poll for events: %s\n",
                    __FUNCTION__, __LINE__, strerrorname_np(errno));

            break;
        }

        if (fds[0].revents & POLLIN) {
            handle_events(li);
        }

        if (fds[1].revents & POLLIN) {
            running = false;
        }
	}

    pthread_mutex_lock(&stop_fd_mutex);
    close(stop_fd);
    stop_fd = -1;
    pthread_mutex_unlock(&stop_fd_mutex);

    libinput_unref(li);
    udev_unref(udev);

    return true;
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
