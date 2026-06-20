#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Intrinsic.h>

#include <uiohook.h>

#include "input_helper.h"
#include "logger.h"

static XtAppContext xt_context;
static Display *xt_disp;

static pthread_mutex_t xrandr_mutex = PTHREAD_MUTEX_INITIALIZER;
static XRRScreenResources *xrandr_resources = NULL;

static void settings_cleanup_proc(void *arg) {
    if (pthread_mutex_trylock(&xrandr_mutex) == 0) {
        if (xrandr_resources != NULL) {
            XRRFreeScreenResources(xrandr_resources);
            xrandr_resources = NULL;
        }

        if (arg != NULL) {
            XCloseDisplay((Display *) arg);
            arg = NULL;
        }

        pthread_mutex_unlock(&xrandr_mutex);
    }
}

static void *settings_thread_proc(void *arg) {
    Display *settings_disp = XOpenDisplay(XDisplayName(NULL));;
    if (settings_disp != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: %s\n",
                __FUNCTION__, __LINE__, "XOpenDisplay success.");

        pthread_cleanup_push(settings_cleanup_proc, settings_disp);

        int event_base = 0;
        int error_base = 0;
        if (XRRQueryExtension(settings_disp, &event_base, &error_base)) {
            Window root = XDefaultRootWindow(settings_disp);
            unsigned long event_mask = RRScreenChangeNotifyMask;
            XRRSelectInput(settings_disp, root, event_mask);

            XEvent ev;

            while(settings_disp != NULL) {
                XNextEvent(settings_disp, &ev);

                if (ev.type == event_base + RRScreenChangeNotifyMask) {
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Received XRRScreenChangeNotifyEvent.\n",
                            __FUNCTION__, __LINE__);

                    pthread_mutex_lock(&xrandr_mutex);
                    if (xrandr_resources != NULL) {
                        XRRFreeScreenResources(xrandr_resources);
                    }

                    xrandr_resources = XRRGetScreenResources(settings_disp, root);
                    if (xrandr_resources == NULL) {
                        logger(LOG_LEVEL_WARN, "%s [%u]: XRandR could not get screen resources!\n",
                                __FUNCTION__, __LINE__);
                    }
                    pthread_mutex_unlock(&xrandr_mutex);
                } else {
                    logger(LOG_LEVEL_WARN, "%s [%u]: XRandR is not currently available!\n",
                            __FUNCTION__, __LINE__);
                }
            }
        }

        // Execute the thread cleanup handler.
        pthread_cleanup_pop(1);

    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XOpenDisplay failure!\n",
                __FUNCTION__, __LINE__);
    }

    return NULL;
}

screen_data* hook_create_screen_info(unsigned char *count) {
    *count = 0;
    screen_data *screens = NULL;

    // Check and make sure we could connect to the x server.
    if (helper_disp != NULL) {
        pthread_mutex_lock(&xrandr_mutex);

        if (xrandr_resources != NULL) {
            int xrandr_count = xrandr_resources->ncrtc;
            if (xrandr_count > UINT8_MAX) {
                *count = UINT8_MAX;

                logger(LOG_LEVEL_WARN, "%s [%u]: Screen count overflow detected!\n",
                        __FUNCTION__, __LINE__);
            } else {
                *count = (uint8_t) xrandr_count;
            }

            screens = malloc(sizeof(screen_data) * xrandr_count);

            if (screens != NULL) {
                for (int i = 0; i < xrandr_count; i++) {
                    XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(helper_disp, xrandr_resources, xrandr_resources->crtcs[i]);

                    if (crtc_info != NULL) {
                        screens[i] = (screen_data) {
                            .number = i + 1,
                            .x = crtc_info->x,
                            .y = crtc_info->y,
                            .width = crtc_info->width,
                            .height = crtc_info->height
                        };

                        XRRFreeCrtcInfo(crtc_info);
                    } else {
                        logger(LOG_LEVEL_WARN, "%s [%u]: XRandr failed to return crtc information! (%#X)\n",
                                __FUNCTION__, __LINE__, xrandr_resources->crtcs[i]);
                    }
                }
            }
        }

        pthread_mutex_unlock(&xrandr_mutex);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    return screens;
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
    unload_input_helper();

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
