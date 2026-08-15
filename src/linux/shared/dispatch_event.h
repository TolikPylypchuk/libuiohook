#ifndef SHARED_DISPATCH_EVENT_H
#define SHARED_DISPATCH_EVENT_H

#include <stdbool.h>

#include <libinput.h>

/* Dispatches the event which reports that the hook has been enabled. */
void dispatch_hook_enabled();

/* Dispatches the event which reports that the hook has been disabled. */
void dispatch_hook_disabled();

/* Translates a libinput event into a uiohook event and dispatches it. */
void dispatch_libinput_event(struct libinput_event *event, bool emulated);

#endif
