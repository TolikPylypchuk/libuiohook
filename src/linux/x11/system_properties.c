#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Intrinsic.h>

#include <uiohook.h>

#include "input_helper.h"
#include "logger.h"
#include "system_properties.h"

static XtAppContext xt_context;
static Display *xt_disp;

static pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;
static screen_data *screens = NULL;
static uint8_t screen_count = 0;
static uint16_t desktop_width = 0;
static uint16_t desktop_height = 0;

uint32_t hook_get_optional_feature_support() {
    return UIOHOOK_FEATURE_KEY_TYPED_EVENTS
        | UIOHOOK_FEATURE_POST_TEXT
        | UIOHOOK_FEATURE_ABSOLUTE_MOUSE_MOVEMENT
        | UIOHOOK_FEATURE_ABSOLUTE_MOUSE_BUTTON_COORDS
        | UIOHOOK_FEATURE_POINTER_PROPERTIES;
}

static void publish_screens(screen_data *new_screens, uint8_t new_count, uint16_t width, uint16_t height) {
    pthread_mutex_lock(&screen_mutex);

    free(screens);

    screens = new_screens;
    screen_count = new_count;
    desktop_width = width;
    desktop_height = height;

    pthread_mutex_unlock(&screen_mutex);
}

static void refresh_screens(Display *disp, Window root, bool poll_hardware) {
    XRRScreenResources *resources = poll_hardware
        ? XRRGetScreenResources(disp, root)
        : XRRGetScreenResourcesCurrent(disp, root);

    if (resources == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: XRandR could not get screen resources!\n",
                __FUNCTION__, __LINE__);

        return;
    }

    screen_data *new_screens = NULL;
    uint8_t new_count = 0;

    int32_t min_x = INT32_MAX, min_y = INT32_MAX, max_x = INT32_MIN, max_y = INT32_MIN;

    if (resources->ncrtc > 0) {
        new_screens = malloc(sizeof(screen_data) * resources->ncrtc);
        if (new_screens == NULL) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for the screen layout!\n",
                    __FUNCTION__, __LINE__);

            XRRFreeScreenResources(resources);
            return;
        }
    }

    for (int i = 0; i < resources->ncrtc; i++) {
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(disp, resources, resources->crtcs[i]);

        if (crtc_info == NULL) {
            logger(LOG_LEVEL_WARN, "%s [%u]: XRandR failed to return crtc information! (%#lX)\n",
                    __FUNCTION__, __LINE__, resources->crtcs[i]);

            continue;
        }

        // Disabled crtcs report no mode and a zero size, so ignore them.
        if (crtc_info->mode != None && crtc_info->width > 0 && crtc_info->height > 0) {
            if (new_count == UINT8_MAX) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Screen count overflow detected!\n",
                        __FUNCTION__, __LINE__);

                XRRFreeCrtcInfo(crtc_info);
                break;
            }

            new_screens[new_count] = (screen_data) {
                .number = new_count + 1,
                .x = crtc_info->x,
                .y = crtc_info->y,
                .width = crtc_info->width,
                .height = crtc_info->height
            };

            new_count++;

            if (crtc_info->x < min_x) {
                min_x = crtc_info->x;
            }

            if (crtc_info->y < min_y) {
                min_y = crtc_info->y;
            }

            if ((int32_t) (crtc_info->x + crtc_info->width) > max_x) {
                max_x = crtc_info->x + crtc_info->width;
            }

            if ((int32_t) (crtc_info->y + crtc_info->height) > max_y) {
                max_y = crtc_info->y + crtc_info->height;
            }
        }

        XRRFreeCrtcInfo(crtc_info);
    }

    if (new_count == 0) {
        free(new_screens);
        new_screens = NULL;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Resolved %u screen(s) over %i x %i.\n",
            __FUNCTION__, __LINE__, new_count, new_count > 0 ? max_x - min_x : 0, new_count > 0 ? max_y - min_y : 0);

    publish_screens(
        new_screens,
        new_count,
        new_count > 0 ? (uint16_t) (max_x - min_x) : 0,
        new_count > 0 ? (uint16_t) (max_y - min_y) : 0);

    XRRFreeScreenResources(resources);
}

static void settings_cleanup_proc(void *arg) {
    if (arg != NULL) {
        XCloseDisplay((Display *) arg);
    }
}

static void *settings_thread_proc(void *arg) {
    Display *settings_disp = XOpenDisplay(XDisplayName(NULL));
    if (settings_disp != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: %s\n",
                __FUNCTION__, __LINE__, "XOpenDisplay success.");

        pthread_cleanup_push(settings_cleanup_proc, settings_disp);

        int event_base = 0;
        int error_base = 0;
        if (XRRQueryExtension(settings_disp, &event_base, &error_base)) {
            Window root = XDefaultRootWindow(settings_disp);
            XRRSelectInput(settings_disp, root, RRScreenChangeNotifyMask);

            refresh_screens(settings_disp, root, false);

            XEvent ev;

            while (true) {
                XNextEvent(settings_disp, &ev);

                if (ev.type == event_base + RRScreenChangeNotify) {
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Received XRRScreenChangeNotifyEvent.\n",
                            __FUNCTION__, __LINE__);

                    XRRUpdateConfiguration(&ev);
                    refresh_screens(settings_disp, root, true);
                }
            }
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: XRandR is not currently available!\n",
                    __FUNCTION__, __LINE__);
        }

        // Execute the thread cleanup handler.
        pthread_cleanup_pop(1);
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XOpenDisplay failure!\n",
                __FUNCTION__, __LINE__);
    }

    return NULL;
}

bool get_desktop_bounds(uint16_t *width, uint16_t *height) {
    pthread_mutex_lock(&screen_mutex);

    bool available = screen_count > 0;
    if (available) {
        *width = desktop_width;
        *height = desktop_height;
    }

    pthread_mutex_unlock(&screen_mutex);

    return available;
}

bool get_screen_origin(int16_t *x, int16_t *y) {
    pthread_mutex_lock(&screen_mutex);

    // Coordinates are relative to the first screen's origin on multi-monitor layouts only.
    bool adjusted = screen_count > 1;
    if (adjusted) {
        *x = screens[0].x;
        *y = screens[0].y;
    }

    pthread_mutex_unlock(&screen_mutex);

    return adjusted;
}

screen_data* hook_create_screen_info(unsigned char *count) {
    *count = 0;
    screen_data *result = NULL;

    pthread_mutex_lock(&screen_mutex);

    if (screen_count > 0) {
        result = malloc(sizeof(screen_data) * screen_count);

        if (result != NULL) {
            memcpy(result, screens, sizeof(screen_data) * screen_count);
            *count = screen_count;
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for the screen information!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: The screen layout is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    pthread_mutex_unlock(&screen_mutex);

    return result;
}

long int hook_get_auto_repeat_rate() {
    bool successful = false;
    long int value = -1;
    unsigned int delay = 0, rate = 0;

    // Check and make sure we could connect to the X server.
    if (helper_disp != NULL) {
        // Attempt to acquire the keyboard auto repeat rate using the XKB extension.
        if (!successful) {
            successful = XkbGetAutoRepeatRate(helper_disp, XkbUseCoreKbd, &delay, &rate);

            if (successful) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: XkbGetAutoRepeatRate: %u.\n",
                        __FUNCTION__, __LINE__, rate);
            }
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    if (successful) {
        value = (long int) rate;
    }

    return value;
}

long int hook_get_auto_repeat_delay() {
    bool successful = false;
    long int value = -1;
    unsigned int delay = 0, rate = 0;

    // Check and make sure we could connect to the X server.
    if (helper_disp != NULL) {
        // Attempt to acquire the keyboard auto repeat rate using the XKB extension.
        if (!successful) {
            successful = XkbGetAutoRepeatRate(helper_disp, XkbUseCoreKbd, &delay, &rate);

            if (successful) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: XkbGetAutoRepeatRate: %u.\n",
                        __FUNCTION__, __LINE__, delay);
            }
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    if (successful) {
        value = (long int) delay;
    }

    return value;
}

long int hook_get_pointer_acceleration_multiplier() {
    long int value = -1;
    int accel_numerator, accel_denominator, threshold;

    // Check and make sure we could connect to the x server.
    if (helper_disp != NULL) {
        XGetPointerControl(helper_disp, &accel_numerator, &accel_denominator, &threshold);
        if (accel_denominator >= 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: XGetPointerControl: %i.\n",
                    __FUNCTION__, __LINE__, accel_denominator);

            value = (long int) accel_denominator;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    return value;
}

long int hook_get_pointer_acceleration_threshold() {
    long int value = -1;
    int accel_numerator, accel_denominator, threshold;

    // Check and make sure we could connect to the x server.
    if (helper_disp != NULL) {
        XGetPointerControl(helper_disp, &accel_numerator, &accel_denominator, &threshold);
        if (threshold >= 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: XGetPointerControl: %i.\n",
                    __FUNCTION__, __LINE__, threshold);

            value = (long int) threshold;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    return value;
}

long int hook_get_pointer_sensitivity() {
    long int value = -1;
    int accel_numerator, accel_denominator, threshold;

    // Check and make sure we could connect to the x server.
    if (helper_disp != NULL) {
        XGetPointerControl(helper_disp, &accel_numerator, &accel_denominator, &threshold);
        if (accel_numerator >= 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: XGetPointerControl: %i.\n",
                    __FUNCTION__, __LINE__, accel_numerator);

            value = (long int) accel_numerator;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    return value;
}

long int hook_get_multi_click_time() {
    long int value = 200;
    int click_time;
    bool successful = false;

    // Check and make sure we could connect to the X server.
    if (xt_disp != NULL) {
        // Try and use the Xt extention to get the current multi-click.
        if (!successful) {
            // Fall back to the X Toolkit extension if available and other efforts failed.
            click_time = XtGetMultiClickTime(xt_disp);
            if (click_time >= 0) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: XtGetMultiClickTime: %i.\n",
                        __FUNCTION__, __LINE__, click_time);

                successful = true;
            }
        }
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: %s\n",
                __FUNCTION__, __LINE__, "XDisplay xt_disp is unavailable!");
    }

    // Check and make sure we could connect to the x server.
    if (helper_disp != NULL) {
        // Try and acquire the multi-click time from the user defined X defaults.
        if (!successful) {
            char *xprop = XGetDefault(helper_disp, "*", "multiClickTime");
            if (xprop != NULL && sscanf(xprop, "%4i", &click_time) != EOF) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: X default 'multiClickTime' property: %i.\n",
                        __FUNCTION__, __LINE__, click_time);

                successful = true;
            }
        }

        if (!successful) {
            char *xprop = XGetDefault(helper_disp, "OpenWindows", "MultiClickTimeout");
            if (xprop != NULL && sscanf(xprop, "%4i", &click_time) != EOF) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: X default 'MultiClickTimeout' property: %i.\n",
                        __FUNCTION__, __LINE__, click_time);

                successful = true;
            }
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    if (successful) {
        value = (long int) click_time;
    }

    return value;
}

// Create a shared object constructor.
__attribute__ ((constructor))
void on_library_load() {
    // Make sure we are initialized for threading.
    XInitThreads();

    // Open local display.
    helper_disp = XOpenDisplay(XDisplayName(NULL));
    if (helper_disp == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: %s\n",
                __FUNCTION__, __LINE__, "XOpenDisplay failure!");
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: %s\n",
                __FUNCTION__, __LINE__, "XOpenDisplay success.");

        // Refresh screens right away, so we don't wait for the settings thread to initialize.
        refresh_screens(helper_disp, XDefaultRootWindow(helper_disp), false);
    }

    // Create the thread attribute.
    pthread_attr_t settings_thread_attr;
    pthread_attr_init(&settings_thread_attr);

    pthread_t settings_thread_id;
    if (pthread_create(&settings_thread_id, &settings_thread_attr, settings_thread_proc, NULL) == 0) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Successfully created settings thread.\n",
                __FUNCTION__, __LINE__);
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create settings thread!\n",
                __FUNCTION__, __LINE__);
    }

    // Make sure the thread attribute is removed.
    pthread_attr_destroy(&settings_thread_attr);

    // Open XT display.
    XtToolkitInitialize();
    xt_context = XtCreateApplicationContext();

    int argc = 0;
    char ** argv = { NULL };
    xt_disp = XtOpenDisplay(xt_context, NULL, "UIOHook", "libuiohook", NULL, 0, &argc, argv);
}

// Create a shared object destructor.
__attribute__ ((destructor))
void on_library_unload() {
    publish_screens(NULL, 0, 0, 0);

    if (xt_disp != NULL) {
        XtCloseDisplay(xt_disp);
    }

    if (xt_context != NULL) {
        XtDestroyApplicationContext(xt_context);
    }

    if (helper_disp != NULL) {
        XCloseDisplay(helper_disp);
        helper_disp = NULL;
    }
}
