#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>


static void logger_proc(unsigned int level, void *user_data, const char *format, va_list args) {
    switch (level) {
        case LOG_LEVEL_INFO:
            vfprintf(stdout, format, args);
            break;

        case LOG_LEVEL_WARN:
        case LOG_LEVEL_ERROR:
            vfprintf(stderr, format, args);
            break;
    }
}

static void logger(unsigned int level, const char *format, ...) {
    va_list args;

    va_start(args, format);
    logger_proc(level, NULL, format, args);
    va_end(args);
}

int main() {
    // Disable the logger.
    hook_set_logger_proc(&logger_proc, NULL);

    uint32_t features = hook_get_optional_feature_support();
    logger(LOG_LEVEL_INFO, "Optional Features:\t0x%02X\n", features);
    logger(LOG_LEVEL_INFO, "\tEvent Suppression:\t%s\n",
        features & UIOHOOK_FEATURE_EVENT_SUPPRESSION ? "yes" : "no");
    logger(LOG_LEVEL_INFO, "\tKey Typed Events:\t%s\n",
        features & UIOHOOK_FEATURE_KEY_TYPED_EVENTS ? "yes" : "no");
    logger(LOG_LEVEL_INFO, "\tPost Text:\t\t%s\n",
        features & UIOHOOK_FEATURE_POST_TEXT ? "yes" : "no");
    logger(LOG_LEVEL_INFO, "\tAbsolute Mouse Movement:\t%s\n",
        features & UIOHOOK_FEATURE_ABSOLUTE_MOUSE_MOVEMENT ? "yes" : "no");
    logger(LOG_LEVEL_INFO, "\tAbsolute Mouse Button Coords:\t%s\n",
        features & UIOHOOK_FEATURE_ABSOLUTE_MOUSE_BUTTON_COORDS ? "yes" : "no");
    logger(LOG_LEVEL_INFO, "\tPointer Properties:\t%s\n",
        features & UIOHOOK_FEATURE_POINTER_PROPERTIES ? "yes" : "no");
    logger(LOG_LEVEL_INFO, "\n");

    logger(LOG_LEVEL_INFO, "Loaded Linux Back-end:\t%i\n\n", hook_get_loaded_linux_backend());

    // Retrieves current monitor layout and size.
    unsigned char count;
    screen_data* monitors = hook_create_screen_info(&count);
    logger(LOG_LEVEL_INFO, "Monitors Found:\t%u\n", count);
    for (int i = 0; i < count; i++) {
        logger(LOG_LEVEL_INFO, "\t%3u) %4u x %-4u (%5d, %-5d)\n",
            monitors[i].number,
            monitors[i].width, monitors[i].height,
            monitors[i].x, monitors[i].y);
    }
    logger(LOG_LEVEL_INFO, "\n");

    // Retrieves the keyboard auto repeat rate.
    long int repeat_rate = hook_get_auto_repeat_rate();
    if (repeat_rate >= 0) {
        logger(LOG_LEVEL_INFO, "Auto Repeat Rate:\t%ld\n", repeat_rate);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire keyboard auto repeat rate!\n");
    }

    // Retrieves the keyboard auto repeat delay.
    long int repeat_delay = hook_get_auto_repeat_delay();
    if (repeat_delay >= 0) {
        logger(LOG_LEVEL_INFO, "Auto Repeat Delay:\t%ld\n", repeat_delay);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire keyboard auto repeat delay!\n");
    }

    // Retrieves the mouse acceleration multiplier.
    long int acceleration_multiplier = hook_get_pointer_acceleration_multiplier();
    if (acceleration_multiplier >= 0) {
        logger(LOG_LEVEL_INFO, "Mouse Acceleration Multiplier:\t%ld\n", acceleration_multiplier);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse acceleration multiplier!\n");
    }

    // Retrieves the mouse acceleration threshold.
    long int acceleration_threshold = hook_get_pointer_acceleration_threshold();
    if (acceleration_threshold >= 0) {
        logger(LOG_LEVEL_INFO, "Mouse Acceleration Threshold:\t%ld\n", acceleration_threshold);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse acceleration threshold!\n");
    }

    // Retrieves the mouse sensitivity.
    long int sensitivity = hook_get_pointer_sensitivity();
    if (sensitivity >= 0) {
        logger(LOG_LEVEL_INFO, "Mouse Sensitivity:\t%ld\n", sensitivity);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse sensitivity value!\n");
    }

    // Retrieves the double/triple click interval.
    long int click_time = hook_get_multi_click_time();
    if (click_time >= 0) {
        logger(LOG_LEVEL_INFO, "Multi-Click Time:\t%ld\n", click_time);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse multi-click time!\n");
    }

    return EXIT_SUCCESS;
}
