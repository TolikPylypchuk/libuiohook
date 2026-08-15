#ifndef BACKEND_H
#define BACKEND_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

/* Translates an evdev key code to its unicode representation for key typed events.
 * Returns the number of characters which were written to the buffer. */
size_t backend_key_to_unicode(uint16_t evdev_code, uint16_t modifier_mask, wchar_t *buffer, size_t length);

/* Gets the current pointer position in desktop coordinates.
 * Returns false if the back-end cannot provide a position. */
bool backend_get_pointer_position(int16_t *x, int16_t *y);

/* Gets the size of the desktop bounding box.
 * Returns false if the back-end cannot provide it. */
bool backend_get_desktop_bounds(uint16_t *width, uint16_t *height);

#endif
