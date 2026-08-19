#ifndef INPUT_HELPER_H
#define INPUT_HELPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <X11/Xlib.h>

// X11 key codes are offset by 8 from the evdev key codes which they are derived from.
#define EVDEV_KEYCODE_OFFSET 8

// Helper display used by input helper, properties and post event.
extern Display *helper_disp;

/* Converts a X11 key event to its UTF-16 representation using the given input context. */
extern size_t event_to_unicode(XKeyEvent *x_event, XIC xic, uint16_t *surrogate, size_t length);

#endif
