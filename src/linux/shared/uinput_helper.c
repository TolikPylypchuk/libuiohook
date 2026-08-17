#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <libudev.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <logger.h>
#include <uiohook.h>

#include "device_procs.h"
#include "uinput_helper.h"

#define UINPUT_PATH                 "/dev/uinput"
#define VIRTUAL_INPUT_PATH          "/sys/devices/virtual/input/"

// 0x7569 is 'ui' in ASCII.
#define VIRTUAL_DEVICE_VENDOR       0x7569
#define VIRTUAL_DEVICE_VERSION      0x0001

#define VIRTUAL_KEYBOARD_TYPE       "virtual keyboard"
#define VIRTUAL_KEYBOARD_PRODUCT    0x0001

#define VIRTUAL_POINTER_TYPE        "virtual pointer"
#define VIRTUAL_POINTER_PRODUCT     0x0002

#define DEFAULT_APPLICATION_NAME    "libuiohook"

#define APPLICATION_NAME_MAX        ((int) (UINPUT_MAX_NAME_SIZE - sizeof(" " VIRTUAL_KEYBOARD_TYPE)))

#define DEVICE_INIT_TIMEOUT_MS      2000
#define DEVICE_INIT_POLL_MS         10

#define DEVICE_SETTLE_MS            100

typedef struct _uinput_device {
    const char *type;
    uint16_t product;
    bool (*configure)(int fd);
    int fd;
    char name[UINPUT_MAX_NAME_SIZE];
} uinput_device;

static bool configure_keyboard(int fd);
static bool configure_pointer(int fd);

static uinput_device devices[] = {
    [VIRTUAL_DEVICE_KEYBOARD] = {
        .type = VIRTUAL_KEYBOARD_TYPE,
        .product = VIRTUAL_KEYBOARD_PRODUCT,
        .configure = configure_keyboard,
        .fd = -1,
        .name = DEFAULT_APPLICATION_NAME " " VIRTUAL_KEYBOARD_TYPE
    },
    [VIRTUAL_DEVICE_POINTER] = {
        .type = VIRTUAL_POINTER_TYPE,
        .product = VIRTUAL_POINTER_PRODUCT,
        .configure = configure_pointer,
        .fd = -1,
        .name = DEFAULT_APPLICATION_NAME " " VIRTUAL_POINTER_TYPE
    }
};

#define DEVICE_COUNT (sizeof(devices) / sizeof(devices[0]))

static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned int reference_count = 0;

static device_procs procs;

static void sleep_ms(unsigned int milliseconds) {
    struct timespec ts = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000
    };

    nanosleep(&ts, NULL);
}

static bool configure_keyboard(int fd) {
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0
            || ioctl(fd, UI_SET_EVBIT, EV_MSC) < 0
            || ioctl(fd, UI_SET_MSCBIT, MSC_SCAN) < 0) {
        return false;
    }

    for (uint16_t code = KEY_RESERVED + 1; code <= KEY_MAX; code++) {
        if (ioctl(fd, UI_SET_KEYBIT, code) < 0) {
            return false;
        }
    }

    return true;
}

static bool configure_pointer(int fd) {
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0
            || ioctl(fd, UI_SET_EVBIT, EV_REL) < 0
            || ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) {
        return false;
    }

    for (uint16_t code = BTN_LEFT; code <= BTN_TASK; code++) {
        if (ioctl(fd, UI_SET_KEYBIT, code) < 0) {
            return false;
        }
    }

    const uint16_t axes[] = {
        REL_X, REL_Y, REL_WHEEL, REL_HWHEEL, REL_WHEEL_HI_RES, REL_HWHEEL_HI_RES
    };

    for (unsigned int i = 0; i < sizeof(axes) / sizeof(axes[0]); i++) {
        if (ioctl(fd, UI_SET_RELBIT, axes[i]) < 0) {
            return false;
        }
    }

    const uint16_t absolute_axes[] = { ABS_X, ABS_Y };

    for (unsigned int i = 0; i < sizeof(absolute_axes) / sizeof(absolute_axes[0]); i++) {
        struct uinput_abs_setup setup = {
            .code = absolute_axes[i],
            .absinfo = { .minimum = 0, .maximum = ABSOLUTE_AXIS_MAX }
        };

        if (ioctl(fd, UI_ABS_SETUP, &setup) < 0) {
            return false;
        }
    }

    return true;
}

static int create_device(uinput_device *device, const char * const application_name) {
    snprintf(device->name, sizeof(device->name), "%.*s %s",
            APPLICATION_NAME_MAX, application_name, device->type);

    int fd = open_device(&procs, UINPUT_PATH, O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        if (-fd == EACCES) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: No permission to open %s! "
                    "A udev rule which grants access to it is most likely missing.\n",
                    __FUNCTION__, __LINE__, UINPUT_PATH);
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open %s: %s\n",
                    __FUNCTION__, __LINE__, UINPUT_PATH, strerrorname_np(-fd));
        }

        return UIOHOOK_ERROR_LINUX_OPEN_UINPUT;
    }

    struct uinput_setup setup = {
        .id = {
            .bustype = BUS_VIRTUAL,
            .vendor = VIRTUAL_DEVICE_VENDOR,
            .product = device->product,
            .version = VIRTUAL_DEVICE_VERSION
        }
    };

    strncpy(setup.name, device->name, UINPUT_MAX_NAME_SIZE - 1);

    if (!device->configure(fd) || ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create the %s: %s\n",
                __FUNCTION__, __LINE__, device->name, strerrorname_np(errno));

        close_device(&procs, fd);
        return UIOHOOK_ERROR_LINUX_CREATE_UINPUT_DEVICE;
    }

    device->fd = fd;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Created the %s.\n",
            __FUNCTION__, __LINE__, device->name);

    return UIOHOOK_SUCCESS;
}

static void destroy_device(uinput_device *device) {
    if (device->fd < 0) {
        return;
    }

    if (ioctl(device->fd, UI_DEV_DESTROY) < 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to destroy the %s: %s\n",
                __FUNCTION__, __LINE__, device->name, strerrorname_np(errno));
    }

    close_device(&procs, device->fd);
    device->fd = -1;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Destroyed the %s.\n",
            __FUNCTION__, __LINE__, device->name);
}

static void wait_for_device(struct udev *udev, const uinput_device *device) {
    char sysname[32] = {};

    if (ioctl(device->fd, UI_GET_SYSNAME(sizeof(sysname) - 1), sysname) < 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to get the sys name of the %s: %s\n",
                __FUNCTION__, __LINE__, device->name, strerrorname_np(errno));

        return;
    }

    char syspath[sizeof(VIRTUAL_INPUT_PATH) + sizeof(sysname)];
    snprintf(syspath, sizeof(syspath), "%s%s", VIRTUAL_INPUT_PATH, sysname);

    for (unsigned int elapsed = 0; elapsed < DEVICE_INIT_TIMEOUT_MS; elapsed += DEVICE_INIT_POLL_MS) {
        struct udev_device *udev_device = udev_device_new_from_syspath(udev, syspath);
        bool initialized = udev_device != NULL && udev_device_get_is_initialized(udev_device);

        if (udev_device != NULL) {
            udev_device_unref(udev_device);
        }

        if (initialized) {
            return;
        }

        sleep_ms(DEVICE_INIT_POLL_MS);
    }

    logger(LOG_LEVEL_WARN, "%s [%u]: Timed out waiting for udev to initialize the %s.\n",
            __FUNCTION__, __LINE__, device->name);
}

static void wait_for_devices() {
    struct udev *udev = udev_new();

    if (udev != NULL) {
        for (unsigned int i = 0; i < DEVICE_COUNT; i++) {
            wait_for_device(udev, &devices[i]);
        }

        udev_unref(udev);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to create a udev context!\n",
                __FUNCTION__, __LINE__);
    }

    sleep_ms(DEVICE_SETTLE_MS);
}

int create_virtual_devices(const char * const application_name) {
    pthread_mutex_lock(&device_mutex);

    int status = UIOHOOK_SUCCESS;

    if (reference_count == 0) {
        const char *name = application_name != NULL && application_name[0] != '\0'
            ? application_name
            : DEFAULT_APPLICATION_NAME;

        procs = get_device_procs();

        for (unsigned int i = 0; i < DEVICE_COUNT && status == UIOHOOK_SUCCESS; i++) {
            status = create_device(&devices[i], name);
        }

        if (status == UIOHOOK_SUCCESS) {
            wait_for_devices();
        } else {
            for (unsigned int i = 0; i < DEVICE_COUNT; i++) {
                destroy_device(&devices[i]);
            }
        }
    } else if (reference_count == UINT_MAX) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Virtual device reference count overflow detected!\n",
                __FUNCTION__, __LINE__);

        status = UIOHOOK_FAILURE;
    }

    if (status == UIOHOOK_SUCCESS) {
        reference_count++;
    }

    pthread_mutex_unlock(&device_mutex);

    return status;
}

int destroy_virtual_devices() {
    pthread_mutex_lock(&device_mutex);

    if (reference_count > 0) {
        reference_count--;

        if (reference_count == 0) {
            for (unsigned int i = 0; i < DEVICE_COUNT; i++) {
                destroy_device(&devices[i]);
            }
        }
    }

    pthread_mutex_unlock(&device_mutex);

    return UIOHOOK_SUCCESS;
}

int lock_virtual_devices() {
    pthread_mutex_lock(&device_mutex);

    if (reference_count == 0) {
        pthread_mutex_unlock(&device_mutex);

        logger(LOG_LEVEL_ERROR, "%s [%u]: The virtual devices are not initialized!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_LINUX_VIRTUAL_DEVICES_NOT_INITIALIZED;
    }

    return UIOHOOK_SUCCESS;
}

void unlock_virtual_devices() {
    pthread_mutex_unlock(&device_mutex);
}

int post_virtual_events(virtual_device device, const virtual_event *events, size_t count) {
    if (count > VIRTUAL_EVENT_MAX) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Cannot write %zu events as a single report!\n",
                __FUNCTION__, __LINE__, count);

        return UIOHOOK_FAILURE;
    }

    struct input_event input_events[VIRTUAL_EVENT_MAX + 1] = {};

    for (size_t i = 0; i < count; i++) {
        input_events[i].type = events[i].type;
        input_events[i].code = events[i].code;
        input_events[i].value = events[i].value;
    }

    input_events[count].type = EV_SYN;
    input_events[count].code = SYN_REPORT;

    size_t size = sizeof(struct input_event) * (count + 1);

    if (write(devices[device].fd, input_events, size) != (ssize_t) size) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to write to the %s: %s\n",
                __FUNCTION__, __LINE__, devices[device].name, strerrorname_np(errno));

        return UIOHOOK_ERROR_LINUX_WRITE_UINPUT;
    }

    return UIOHOOK_SUCCESS;
}

int post_virtual_key(uint16_t evdev_code, bool pressed) {
    virtual_event events[] = {
        { .type = EV_MSC, .code = MSC_SCAN, .value = evdev_code },
        { .type = EV_KEY, .code = evdev_code, .value = pressed ? 1 : 0 }
    };

    return post_virtual_events(VIRTUAL_DEVICE_KEYBOARD, events, sizeof(events) / sizeof(events[0]));
}

__attribute__((destructor))
static void unload_virtual_devices() {
    pthread_mutex_lock(&device_mutex);

    if (reference_count > 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: The virtual devices were not destroyed!\n",
                __FUNCTION__, __LINE__);

        reference_count = 0;

        for (unsigned int i = 0; i < DEVICE_COUNT; i++) {
            destroy_device(&devices[i]);
        }
    }

    pthread_mutex_unlock(&device_mutex);
}
