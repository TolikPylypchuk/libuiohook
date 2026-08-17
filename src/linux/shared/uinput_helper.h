#ifndef UINPUT_HELPER_H
#define UINPUT_HELPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VIRTUAL_EVENT_MAX 4
#define ABSOLUTE_AXIS_MAX 65535

typedef enum _virtual_device {
    VIRTUAL_DEVICE_KEYBOARD,
    VIRTUAL_DEVICE_POINTER
} virtual_device;

typedef struct _virtual_event {
    uint16_t type;
    uint16_t code;
    int32_t value;
} virtual_event;

/* Creates the virtual devices if they don't exist yet, and increments their reference count.
 * The application name is only used when the devices are actually created. */
int create_virtual_devices(const char * const application_name);

/* Decrements the reference count of the virtual devices, and destroys them when it reaches zero. */
int destroy_virtual_devices();

/* Locks the virtual devices for posting, and fails if they are not initialized. */
int lock_virtual_devices();

/* Unlocks the virtual devices. */
void unlock_virtual_devices();

/* Writes events to a virtual device, followed by a report. The devices must be locked. */
int post_virtual_events(virtual_device device, const virtual_event *events, size_t count);

/* Presses or releases a key on the virtual keyboard. The devices must be locked. */
int post_virtual_key(uint16_t evdev_code, bool pressed);

#endif
