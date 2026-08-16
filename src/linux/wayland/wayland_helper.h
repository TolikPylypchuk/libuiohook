#ifndef WAYLAND_HELPER_H
#define WAYLAND_HELPER_H

#include <stdbool.h>
#include <stdint.h>

/* Connects to the compositor and starts keeping track of the state which is read from it. */
bool wayland_helper_init();

/* Gets the key repeat rate in repeats per second, or -1 if the compositor didn't report one. */
int32_t wayland_helper_get_repeat_rate();

/* Gets the key repeat delay in milliseconds, or -1 if the compositor didn't report one. */
int32_t wayland_helper_get_repeat_delay();

#endif
