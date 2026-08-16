#define _GNU_SOURCE

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/eventfd.h>

#include <wayland-client.h>

#include <logger.h>
#include <uiohook.h>

#include "monitor_helper.h"
#include "wayland-xdg-output-unstable-v1-client-protocol.h"
#include "wayland_helper.h"

#define WL_SEAT_REPEAT_INFO_VERSION  4
#define WL_KEYBOARD_RELEASE_VERSION  3

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static bool initialized = false;

static struct wl_display *display = NULL;
static struct wl_event_queue *queue = NULL;
static struct wl_registry *registry = NULL;

static struct wl_seat *seat = NULL;
static uint32_t seat_name = 0;
static struct wl_keyboard *keyboard = NULL;

static pthread_mutex_t repeat_mutex = PTHREAD_MUTEX_INITIALIZER;
static int32_t repeat_rate = -1;
static int32_t repeat_delay = -1;

static pthread_t dispatch_thread;
static bool dispatch_thread_running = false;
static int stop_fd = -1;

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
    // The keymap is of no use without key typed events, so close the descriptor.
    close(fd);
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: The key repeat rate is %i/s with a delay of %i ms.\n",
            __FUNCTION__, __LINE__, rate, delay);

    pthread_mutex_lock(&repeat_mutex);

    repeat_rate = rate;
    repeat_delay = delay;

    pthread_mutex_unlock(&repeat_mutex);
}

// Key events are only ever sent to a focused client, and this connection is never focused.

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {
}

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
}

static void keyboard_modifiers(
        void *data,
        struct wl_keyboard *wl_keyboard,
        uint32_t serial,
        uint32_t mods_depressed,
        uint32_t mods_latched,
        uint32_t mods_locked,
        uint32_t group) {
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info
};

static void destroy_keyboard() {
    if (keyboard == NULL) {
        return;
    }

    if (wl_keyboard_get_version(keyboard) >= WL_KEYBOARD_RELEASE_VERSION) {
        wl_keyboard_release(keyboard);
    } else {
        wl_keyboard_destroy(keyboard);
    }

    keyboard = NULL;
}

static void seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard == NULL) {
        keyboard = wl_seat_get_keyboard(wl_seat);

        if (keyboard != NULL) {
            wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
        }
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {
        destroy_keyboard();
    }
}

static void seat_name_event(void *data, struct wl_seat *wl_seat, const char *name) {
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name_event
};

static void destroy_seat() {
    destroy_keyboard();

    if (seat != NULL) {
        wl_seat_destroy(seat);
        seat = NULL;
    }
}

static void add_seat(struct wl_registry *wl_registry, uint32_t name, uint32_t version) {
    if (seat != NULL) {
        return;
    }

    uint32_t bind_version = version < WL_SEAT_REPEAT_INFO_VERSION ? version : WL_SEAT_REPEAT_INFO_VERSION;

    if (bind_version < WL_SEAT_REPEAT_INFO_VERSION) {
        logger(LOG_LEVEL_WARN, "%s [%u]: The seat is too old to report the key repeat settings!\n",
                __FUNCTION__, __LINE__);
    }

    seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, bind_version);
    if (seat == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to bind the seat!\n",
                __FUNCTION__, __LINE__);

        return;
    }

    seat_name = name;
    wl_seat_add_listener(seat, &seat_listener, NULL);
}

typedef struct _global_binding {
    const struct wl_interface *interface;
    void (*bind)(struct wl_registry *registry, uint32_t name, uint32_t version);
} global_binding;

static const global_binding global_bindings[] = {
    { &wl_output_interface,               monitor_helper_add_output         },
    { &zxdg_output_manager_v1_interface,  monitor_helper_add_output_manager },
    { &wl_seat_interface,                 add_seat                          }
};

static void registry_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
    for (size_t i = 0; i < sizeof(global_bindings) / sizeof(global_bindings[0]); i++) {
        if (strcmp(interface, global_bindings[i].interface->name) == 0) {
            global_bindings[i].bind(wl_registry, name, version);
            return;
        }
    }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
    if (seat != NULL && name == seat_name) {
        destroy_seat();
        return;
    }

    monitor_helper_remove_global(name);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove
};

static void *dispatch_thread_proc(void *arg) {
    struct pollfd fds[2];

    fds[0].fd = wl_display_get_fd(display);
    fds[0].events = POLLIN;

    fds[1].fd = stop_fd;
    fds[1].events = POLLIN;

    bool running = true;

    while (running) {
        while (wl_display_prepare_read_queue(display, queue) != 0) {
            if (wl_display_dispatch_queue_pending(display, queue) < 0) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Failed to dispatch the Wayland event queue: %s\n",
                        __FUNCTION__, __LINE__, strerrorname_np(errno));

                return NULL;
            }
        }

        if (wl_display_flush(display) < 0 && errno != EAGAIN) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to flush the Wayland display: %s\n",
                    __FUNCTION__, __LINE__, strerrorname_np(errno));

            wl_display_cancel_read(display);
            return NULL;
        }

        fds[0].revents = 0;
        fds[1].revents = 0;

        if (poll(fds, 2, -1) < 0) {
            wl_display_cancel_read(display);

            if (errno == EINTR) { // We don't care about interruptions here.
                continue;
            }

            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to poll for Wayland events: %s\n",
                    __FUNCTION__, __LINE__, strerrorname_np(errno));

            return NULL;
        }

        if (fds[1].revents & POLLIN) {
            wl_display_cancel_read(display);
            break;
        }

        if (!(fds[0].revents & POLLIN)) {
            wl_display_cancel_read(display);

            if (fds[0].revents & (POLLERR | POLLHUP)) {
                logger(LOG_LEVEL_WARN, "%s [%u]: The connection to the compositor was lost!\n",
                        __FUNCTION__, __LINE__);

                return NULL;
            }

            continue;
        }

        if (wl_display_read_events(display) < 0) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to read the Wayland events: %s\n",
                    __FUNCTION__, __LINE__, strerrorname_np(errno));

            return NULL;
        }

        if (wl_display_dispatch_queue_pending(display, queue) < 0) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to dispatch the Wayland event queue: %s\n",
                    __FUNCTION__, __LINE__, strerrorname_np(errno));

            return NULL;
        }
    }

    return NULL;
}

static int roundtrip() {
    int result = wl_display_roundtrip_queue(display, queue);

    if (result < 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: The Wayland roundtrip has failed: %s\n",
                __FUNCTION__, __LINE__, strerrorname_np(errno));
    }

    return result;
}

static void disconnect() {
    monitor_helper_destroy();
    destroy_seat();

    if (registry != NULL) {
        wl_registry_destroy(registry);
        registry = NULL;
    }

    if (queue != NULL) {
        wl_event_queue_destroy(queue);
        queue = NULL;
    }

    if (display != NULL) {
        wl_display_disconnect(display);
        display = NULL;
    }
}

static void init_helper() {
    display = wl_display_connect(NULL);
    if (display == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to connect to the Wayland display!\n",
                __FUNCTION__, __LINE__);

        return;
    }

    queue = wl_display_create_queue(display);
    if (queue == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create the Wayland event queue!\n",
                __FUNCTION__, __LINE__);

        disconnect();
        return;
    }

    struct wl_display *wrapped_display = wl_proxy_create_wrapper(display);
    if (wrapped_display == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create the Wayland display wrapper!\n",
                __FUNCTION__, __LINE__);

        disconnect();
        return;
    }

    wl_proxy_set_queue((struct wl_proxy *) wrapped_display, queue);
    registry = wl_display_get_registry(wrapped_display);
    wl_proxy_wrapper_destroy(wrapped_display);

    if (registry == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to get the Wayland registry!\n",
                __FUNCTION__, __LINE__);

        disconnect();
        return;
    }

    wl_registry_add_listener(registry, &registry_listener, NULL);

    if (roundtrip() < 0) {
        disconnect();
        return;
    }

    monitor_helper_bind_xdg_outputs();

    if (roundtrip() < 0 || roundtrip() < 0) {
        disconnect();
        return;
    }

    stop_fd = eventfd(0, EFD_NONBLOCK);
    if (stop_fd < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create a stop notification file descriptor: %s\n",
                __FUNCTION__, __LINE__, strerrorname_np(errno));

        disconnect();
        return;
    }

    if (pthread_create(&dispatch_thread, NULL, dispatch_thread_proc, NULL) != 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create the Wayland dispatch thread!\n",
                __FUNCTION__, __LINE__);

        close(stop_fd);
        stop_fd = -1;

        disconnect();
        return;
    }

    dispatch_thread_running = true;
    initialized = true;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Connected to the Wayland display.\n",
            __FUNCTION__, __LINE__);
}

bool wayland_helper_init() {
    pthread_once(&init_once, init_helper);
    return initialized;
}

int32_t wayland_helper_get_repeat_rate() {
    if (!wayland_helper_init()) {
        return -1;
    }

    pthread_mutex_lock(&repeat_mutex);
    int32_t rate = repeat_rate;
    pthread_mutex_unlock(&repeat_mutex);

    return rate;
}

int32_t wayland_helper_get_repeat_delay() {
    if (!wayland_helper_init()) {
        return -1;
    }

    pthread_mutex_lock(&repeat_mutex);
    int32_t delay = repeat_delay;
    pthread_mutex_unlock(&repeat_mutex);

    return delay;
}

__attribute__ ((destructor))
static void wayland_helper_destroy() {
    if (dispatch_thread_running) {
        uint64_t value = 1;
        if (write(stop_fd, &value, sizeof(value)) < 0) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to write to the stop notification file descriptor: %s\n",
                    __FUNCTION__, __LINE__, strerrorname_np(errno));
        }

        pthread_join(dispatch_thread, NULL);
        dispatch_thread_running = false;
    }

    if (stop_fd >= 0) {
        close(stop_fd);
        stop_fd = -1;
    }

    disconnect();
    initialized = false;
}
