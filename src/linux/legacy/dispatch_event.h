#include <X11/Xlib.h>
#include <uiohook.h>

extern bool dispatch_hook_enabled(uint64_t timestamp);

extern bool dispatch_hook_disabled(uint64_t timestamp);

extern bool dispatch_key_press(uint64_t timestamp, XKeyPressedEvent * const x_event);

extern bool dispatch_key_release(uint64_t timestamp, XKeyReleasedEvent * const x_event);

extern bool dispatch_mouse_press(uint64_t timestamp, XButtonEvent * const x_event);

extern bool dispatch_mouse_release(uint64_t timestamp, XButtonEvent * const x_event);

extern bool dispatch_mouse_move(uint64_t timestamp, XMotionEvent * const x_event);
