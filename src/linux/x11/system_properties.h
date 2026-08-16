#ifndef X11_SYSTEM_PROPERTIES_H
#define X11_SYSTEM_PROPERTIES_H

#include <stdbool.h>
#include <stdint.h>

/* Gets the size of the bounding box of every enabled screen. */
bool get_desktop_bounds(uint16_t *width, uint16_t *height);

/* Gets the origin which pointer coordinates are relative to. */
bool get_screen_origin(int16_t *x, int16_t *y);

#endif
