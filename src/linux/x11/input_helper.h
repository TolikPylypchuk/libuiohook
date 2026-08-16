#ifndef INPUT_HELPER_H
#define INPUT_HELPER_H

#include <stdbool.h>
#include <stdint.h>

#include <X11/Xlib.h>

// Virtual button codes that are not defined by X11.
#define Button1     1
#define Button2     2
#define Button3     3
#define WheelUp     4
#define WheelDown   5
#define WheelLeft   6
#define WheelRight  7
#define XButton1    8
#define XButton2    9

// X11 key codes are offset by 8 from the evdev key codes which they are derived from.
#define EVDEV_KEYCODE_OFFSET 8

// Helper display used by input helper, properties and post event.
extern Display *helper_disp;

/* Converts a uiohook virtual key code to the appropriate X11 key code. */
extern KeyCode uiocode_to_keycode(uint16_t uiocode);

/* Converts a X11 key event to its unicode representation using the given input context. */
extern size_t event_to_unicode(XKeyEvent *x_event, XIC xic, wchar_t *surrogate, size_t length);

/* Lookup a X11 buttons possible remapping and return that value. */
extern uint8_t button_map_lookup(uint8_t button);

extern void load_key_mappings();

extern unsigned int get_x11_keycode(const char * keycode_name);

#endif
