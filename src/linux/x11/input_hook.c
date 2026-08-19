#include <stdbool.h>
#include <stdint.h>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <logger.h>
#include <uiohook.h>

#include "backend.h"
#include "input_helper.h"
#include "input_loop.h"
#include "system_properties.h"

static Display *hook_disp = NULL;
static XIM hook_xim = NULL;
static XIC hook_xic = NULL;

static int xkb_event_base = 0;
static bool xkb_events_selected = false;

static bool pointer_position_unavailable_logged = false;

// The input context is only needed for key typed events.
static bool input_context_loaded = false;

static void select_keyboard_mapping_events();

static void load_input_context() {
    if (input_context_loaded) {
        return;
    }

    input_context_loaded = true;

    select_keyboard_mapping_events();

    XSetLocaleModifiers("");
    hook_xim = XOpenIM(hook_disp, NULL, NULL, NULL);
    if (hook_xim == NULL) {
        // Fall back to the internal input method.
        XSetLocaleModifiers("@im=none");
        hook_xim = XOpenIM(hook_disp, NULL, NULL, NULL);
    }

    if (hook_xim == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: XOpenIM() failed!\n",
                __FUNCTION__, __LINE__);

        return;
    }

    Window root = XDefaultRootWindow(hook_disp);
    hook_xic = XCreateIC(hook_xim,
        XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
        XNClientWindow, root,
        XNFocusWindow,  root,
        NULL);

    if (hook_xic == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: XCreateIC() failed!\n",
                __FUNCTION__, __LINE__);
    }
}

static void unload_input_context() {
    if (hook_xic != NULL) {
        XDestroyIC(hook_xic);
        hook_xic = NULL;
    }

    if (hook_xim != NULL) {
        XCloseIM(hook_xim);
        hook_xim = NULL;
    }

    input_context_loaded = false;
    xkb_events_selected = false;
}

// Converts a uiohook modifier mask into the state mask of an X11 key event.
static unsigned int get_x11_modifier_mask(uint16_t modifier_mask) {
    unsigned int state = 0x0000;

    if (modifier_mask & (MASK_SHIFT_L | MASK_SHIFT_R)) { state |= ShiftMask;   }
    if (modifier_mask & (MASK_CTRL_L  | MASK_CTRL_R))  { state |= ControlMask; }
    if (modifier_mask & (MASK_ALT_L   | MASK_ALT_R))   { state |= Mod1Mask;    }
    if (modifier_mask & (MASK_META_L  | MASK_META_R))  { state |= Mod4Mask;    }
    if (modifier_mask & MASK_CAPS_LOCK)                { state |= LockMask;    }
    if (modifier_mask & MASK_NUM_LOCK)                 { state |= Mod2Mask;    }

    return state;
}

// Gets the state mask of an X11 key event, along with the currently active layout group.
static unsigned int get_x11_key_state(uint16_t modifier_mask) {
    XkbStateRec state;

    if (XkbGetState(hook_disp, XkbUseCoreKbd, &state) == Success) {
        return XkbBuildCoreState(state.mods, state.group);
    }

    logger(LOG_LEVEL_WARN, "%s [%u]: XkbGetState() failed!\n",
            __FUNCTION__, __LINE__);

    return get_x11_modifier_mask(modifier_mask);
}

static void select_keyboard_mapping_events() {
    int opcode = 0, error_base = 0;
    int major = XkbMajorVersion, minor = XkbMinorVersion;

    xkb_events_selected = XkbQueryExtension(hook_disp, &opcode, &xkb_event_base, &error_base, &major, &minor)
        && XkbSelectEvents(hook_disp, XkbUseCoreKbd,
                XkbMapNotifyMask | XkbNewKeyboardNotifyMask,
                XkbMapNotifyMask | XkbNewKeyboardNotifyMask);

    if (!xkb_events_selected) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Cannot watch for keyboard mapping changes! "
                "Key typed events will not follow a change of the keyboard layout.\n",
                __FUNCTION__, __LINE__);
    }
}

static void refresh_keyboard_mapping() {
    if (!xkb_events_selected) {
        return;
    }

    XkbEvent event;
    while (XCheckTypedEvent(hook_disp, xkb_event_base, &event.core)) {
        if (event.any.xkb_type == XkbMapNotify) {
            XkbRefreshKeyboardMapping(&event.map);
        }
    }
}

size_t backend_key_to_unicode(uint16_t evdev_code, uint16_t modifier_mask, uint16_t *buffer, size_t length) {
    if (hook_disp == NULL) {
        return 0;
    }

    load_input_context();
    refresh_keyboard_mapping();

    Window root = XDefaultRootWindow(hook_disp);

    // Synthesize an XKeyEvent for the X server to translate.
    XKeyEvent x_event = {
        .type = KeyPress,
        .serial = 0,
        .send_event = False,
        .display = hook_disp,

        .window = root,
        .root = root,
        .subwindow = None,

        .time = CurrentTime,

        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,

        .state = get_x11_key_state(modifier_mask),
        .keycode = evdev_code + EVDEV_KEYCODE_OFFSET,
        .same_screen = True
    };

    return event_to_unicode(&x_event, hook_xic, buffer, length);
}

bool backend_get_pointer_position(int16_t *x, int16_t *y) {
    if (hook_disp == NULL) {
        return false;
    }

    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;

    if (!XQueryPointer(hook_disp, XDefaultRootWindow(hook_disp), &root, &child,
            &root_x, &root_y, &win_x, &win_y, &mask)) {
        if (!pointer_position_unavailable_logged) {
            logger(LOG_LEVEL_WARN, "%s [%u]: The pointer position is unavailable!\n",
                    __FUNCTION__, __LINE__);

            pointer_position_unavailable_logged = true;
        }

        return false;
    }

    int16_t origin_x, origin_y;
    if (get_screen_origin(&origin_x, &origin_y)) {
        root_x -= origin_x;
        root_y -= origin_y;
    }

    *x = (int16_t) root_x;
    *y = (int16_t) root_y;

    return true;
}

bool backend_get_desktop_bounds(uint16_t *width, uint16_t *height) {
    return get_desktop_bounds(width, height);
}

void backend_adjust_absolute_position(int16_t *x, int16_t *y) {
    int16_t origin_x, origin_y;
    if (get_screen_origin(&origin_x, &origin_y)) {
        *x -= origin_x;
        *y -= origin_y;
    }
}

void backend_restore_absolute_position(int16_t *x, int16_t *y) {
    int16_t origin_x, origin_y;
    if (get_screen_origin(&origin_x, &origin_y)) {
        *x += origin_x;
        *y += origin_y;
    }
}

static int run(bool keyboard, bool mouse) {
    hook_disp = XOpenDisplay(XDisplayName(NULL));
    if (hook_disp == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XOpenDisplay failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: XOpenDisplay success.\n",
            __FUNCTION__, __LINE__);

    pointer_position_unavailable_logged = false;

    int status = run_libinput(keyboard, mouse);

    unload_input_context();

    XCloseDisplay(hook_disp);
    hook_disp = NULL;

    return status;
}

int hook_run() {
    return run(true, true);
}

int hook_run_keyboard() {
    return run(true, false);
}

int hook_run_mouse() {
    return run(false, true);
}

int hook_stop() {
    return stop_libinput();
}
