#ifndef DISPATCH_EVENT_H
#define DISPATCH_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#ifndef MAC_CATALYST
#include <ApplicationServices/ApplicationServices.h>
#else
#include <CoreGraphics/CoreGraphics.h>
#endif

extern bool dispatch_hook_enabled(uint64_t timestamp);

extern bool dispatch_hook_disabled(uint64_t timestamp);

extern bool dispatch_key_press(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_key_release(uint64_t timestamp, CGEventRef event_ref);

/* These events are totally undocumented for the CGEvent type, but are required to grab media and caps-lock keys. */
extern bool dispatch_system_key(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_modifier_change(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_button_press(uint64_t timestamp, CGEventRef event_ref, uint16_t button);

extern bool dispatch_button_release(uint64_t timestamp, CGEventRef event_ref, uint16_t button);

extern bool dispatch_mouse_move(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_mouse_wheel(uint64_t timestamp, CGEventRef event_ref);

#endif
