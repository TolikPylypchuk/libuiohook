#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <logger.h>
#include <uiohook.h>

typedef void (*set_logger_proc_t)(logger_t, void *);
typedef void (*set_dispatch_proc_t)(dispatcher_t, void *);

typedef int (*run_t)();
typedef int (*run_keyboard_t)();
typedef int (*run_mouse_t)();
typedef int (*stop_t)();

typedef int (*post_event_t)(uiohook_event * const);
typedef int (*post_events_t)(uiohook_event * const, uint32_t);
typedef int (*post_text_t)(const uint16_t * const);

typedef bool (*is_key_typed_enabled_t)();
typedef void (*set_key_typed_enabled_t)(bool);

typedef bool (*is_ax_api_enabled_t)(bool);
typedef bool (*get_prompt_user_if_ax_api_disabled_t)();
typedef void (*set_prompt_user_if_ax_api_disabled_t)(bool);
typedef uint32_t (*get_ax_poll_frequency_t)();
typedef void (*set_ax_poll_frequency_t)(uint32_t);

typedef uint64_t (*get_post_text_delay_x11_t)();
typedef void (*set_post_text_delay_x11_t)(uint64_t);

typedef screen_data* (*create_screen_info_t)(unsigned char *);
typedef long int (*get_auto_repeat_rate_t)();
typedef long int (*get_auto_repeat_delay_t)();
typedef long int (*get_pointer_acceleration_multiplier_t)();
typedef long int (*get_pointer_acceleration_threshold_t)();
typedef long int (*get_pointer_sensitivity_t)();
typedef long int (*get_multi_click_time_t)();

static void *backend_handle = NULL;
static const char *backend_name = NULL;

static const char const * BACKEND_X11_NAME = "./libuiohook-x11.so";
static const char const * BACKEND_WAYLAND_NAME = "./libuiohook-wayland.so";
static const char const * BACKEND_LEGACY_NAME = "./libuiohook-legacy.so";

static logger_t callback = NULL;
static void *callback_data = NULL;

static set_logger_proc_t set_logger_proc = NULL;
static set_dispatch_proc_t set_dispatch_proc = NULL;

static run_t run = NULL;
static run_keyboard_t run_keyboard = NULL;
static run_mouse_t run_mouse = NULL;
static stop_t stop = NULL;

static post_event_t post_event = NULL;
static post_events_t post_events = NULL;
static post_text_t post_text = NULL;

static is_key_typed_enabled_t is_key_typed_enabled = NULL;
static set_key_typed_enabled_t set_key_typed_enabled = NULL;

static is_ax_api_enabled_t is_ax_api_enabled = NULL;
static get_prompt_user_if_ax_api_disabled_t get_prompt_user_if_ax_api_disabled = NULL;
static set_prompt_user_if_ax_api_disabled_t set_prompt_user_if_ax_api_disabled = NULL;
static get_ax_poll_frequency_t get_ax_poll_frequency = NULL;
static set_ax_poll_frequency_t set_ax_poll_frequency = NULL;

static get_post_text_delay_x11_t get_post_text_delay_x11 = NULL;
static set_post_text_delay_x11_t set_post_text_delay_x11 = NULL;

static create_screen_info_t create_screen_info = NULL;
static get_auto_repeat_rate_t get_auto_repeat_rate = NULL;
static get_auto_repeat_delay_t get_auto_repeat_delay = NULL;
static get_pointer_acceleration_multiplier_t get_pointer_acceleration_multiplier = NULL;
static get_pointer_acceleration_threshold_t get_pointer_acceleration_threshold = NULL;
static get_pointer_sensitivity_t get_pointer_sensitivity = NULL;
static get_multi_click_time_t get_multi_click_time = NULL;

static bool ensure_backend_loaded();

void logger(unsigned int level, const char *format, ...) {
    if (callback != NULL) {
        va_list args;

        va_start(args, format);
        callback(level, callback_data, format, args);
        va_end(args);
    }
}

int hook_get_linux_backend() {
    if (backend_name == NULL) {
        return LINUX_BACKEND_AUTO;
    } else if (strcmp(backend_name, BACKEND_X11_NAME) == 0) {
        return LINUX_BACKEND_X11;
    } else if (strcmp(backend_name, BACKEND_WAYLAND_NAME) == 0) {
        return LINUX_BACKEND_WAYLAND;
    } else if (strcmp(backend_name, BACKEND_LEGACY_NAME) == 0) {
        return LINUX_BACKEND_LEGACY;
    } else {
        return LINUX_BACKEND_AUTO;
    }
}

bool hook_set_linux_backend(int backend) {
    if (backend_handle != NULL) {
        return false;
    }

    switch (backend) {
        case LINUX_BACKEND_X11:
            backend_name = BACKEND_X11_NAME;
            break;
        case LINUX_BACKEND_WAYLAND:
            backend_name = BACKEND_WAYLAND_NAME;
            break;
        case LINUX_BACKEND_LEGACY:
            backend_name = BACKEND_LEGACY_NAME;
            break;
        default:
            backend_name = NULL;
            break;
    }

    return true;
}

void hook_set_logger_proc(logger_t logger_proc, void *user_data) {
    callback = logger_proc;
    callback_data = user_data;

    if (!ensure_backend_loaded()) {
        return;
    }

    set_logger_proc(logger_proc, user_data);
}

void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    if (!ensure_backend_loaded()) {
        return;
    }

    set_dispatch_proc(dispatch_proc, user_data);
}

int hook_run() {
    if (!ensure_backend_loaded()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return run();
}

int hook_run_keyboard() {
    if (!ensure_backend_loaded()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return run_keyboard();
}

int hook_run_mouse() {
    if (!ensure_backend_loaded()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return run_mouse();
}

int hook_stop() {
    if (!ensure_backend_loaded()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return stop();
}

int hook_post_event(uiohook_event * const event) {
    if (!ensure_backend_loaded()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return post_event(event);
}

int hook_post_events(uiohook_event * const events, uint32_t size) {
    if (!ensure_backend_loaded()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return post_events(events, size);
}

int hook_post_text(const uint16_t * const text) {
    if (!ensure_backend_loaded()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return post_text(text);
}

bool hook_is_key_typed_enabled() {
    if (!ensure_backend_loaded()) {
        return false;
    }

    return is_key_typed_enabled();
}

void hook_set_key_typed_enabled(bool enabled) {
    if (!ensure_backend_loaded()) {
        return;
    }

    set_key_typed_enabled(enabled);
}

bool hook_is_ax_api_enabled(bool promptUserIfDisabled) {
    if (!ensure_backend_loaded()) {
        return false;
    }

    return is_ax_api_enabled(promptUserIfDisabled);
}

bool hook_get_prompt_user_if_ax_api_disabled() {
    if (!ensure_backend_loaded()) {
        return false;
    }

    return get_prompt_user_if_ax_api_disabled();
}

void hook_set_prompt_user_if_ax_api_disabled(bool promptUserIfDisabled) {
    if (!ensure_backend_loaded()) {
        return;
    }

    set_prompt_user_if_ax_api_disabled(promptUserIfDisabled);
}

uint32_t hook_get_ax_poll_frequency() {
    if (!ensure_backend_loaded()) {
        return 0;
    }

    return get_ax_poll_frequency();
}

void hook_set_ax_poll_frequency(uint32_t frequency) {
    if (!ensure_backend_loaded()) {
        return;
    }

    set_ax_poll_frequency(frequency);
}

uint64_t hook_get_post_text_delay_x11() {
    if (!ensure_backend_loaded()) {
        return 0;
    }

    return get_post_text_delay_x11();
}

void hook_set_post_text_delay_x11(uint64_t delay) {
    if (!ensure_backend_loaded()) {
        return;
    }

    set_post_text_delay_x11(delay);
}

screen_data* hook_create_screen_info(unsigned char *count) {
    if (!ensure_backend_loaded()) {
        return NULL;
    }

    return create_screen_info(count);
}

long int hook_get_auto_repeat_rate() {
    if (!ensure_backend_loaded()) {
        return -1;
    }

    return get_auto_repeat_rate();
}

long int hook_get_auto_repeat_delay() {
    if (!ensure_backend_loaded()) {
        return -1;
    }

    return get_auto_repeat_delay();
}

long int hook_get_pointer_acceleration_multiplier() {
    if (!ensure_backend_loaded()) {
        return -1;
    }

    return get_pointer_acceleration_multiplier();
}

long int hook_get_pointer_acceleration_threshold() {
    if (!ensure_backend_loaded()) {
        return -1;
    }

    return get_pointer_acceleration_threshold();
}

long int hook_get_pointer_sensitivity() {
    if (!ensure_backend_loaded()) {
        return -1;
    }

    return get_pointer_sensitivity();
}

long int hook_get_multi_click_time() {
    if (!ensure_backend_loaded()) {
        return -1;
    }

    return get_multi_click_time();
}

static const char *get_backend_name() {
    const char *session_type = getenv("XDG_SESSION_TYPE");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");

    bool use_wayland =
        session_type != NULL && strcasecmp(session_type, "wayland") == 0 ||
        wayland_display != NULL && wayland_display[0] != '\0';

    if (use_wayland) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Using the Wayland backend.\n",
                __FUNCTION__, __LINE__);

        return BACKEND_WAYLAND_NAME;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Using the X11 backend.\n",
                __FUNCTION__, __LINE__);

        return BACKEND_X11_NAME;
    }
}

static bool load_backend_symbols() {
    set_logger_proc = (set_logger_proc_t) dlsym(backend_handle, "hook_set_logger_proc");
    if (set_logger_proc == NULL) {
        return false;
    }

    set_dispatch_proc = (set_dispatch_proc_t) dlsym(backend_handle, "hook_set_dispatch_proc");
    if (set_dispatch_proc == NULL) {
        return false;
    }

    run = (run_t) dlsym(backend_handle, "hook_run");
    if (run == NULL) {
        return false;
    }

    run_keyboard = (run_keyboard_t) dlsym(backend_handle, "hook_run_keyboard");
    if (run_keyboard == NULL) {
        return false;
    }

    run_mouse = (run_mouse_t) dlsym(backend_handle, "hook_run_mouse");
    if (run_mouse == NULL) {
        return false;
    }

    stop = (stop_t) dlsym(backend_handle, "hook_stop");
    if (stop == NULL) {
        return false;
    }

    post_event = (post_event_t) dlsym(backend_handle, "hook_post_event");
    if (post_event == NULL) {
        return false;
    }

    post_events = (post_events_t) dlsym(backend_handle, "hook_post_events");
    if (post_events == NULL) {
        return false;
    }

    post_text = (post_text_t) dlsym(backend_handle, "hook_post_text");
    if (post_text == NULL) {
        return false;
    }

    is_key_typed_enabled = (is_key_typed_enabled_t) dlsym(backend_handle, "hook_is_key_typed_enabled");
    if (is_key_typed_enabled == NULL) {
        return false;
    }

    set_key_typed_enabled = (set_key_typed_enabled_t) dlsym(backend_handle, "hook_set_key_typed_enabled");
    if (set_key_typed_enabled == NULL) {
        return false;
    }

    is_ax_api_enabled = (is_ax_api_enabled_t) dlsym(backend_handle, "hook_is_ax_api_enabled");
    if (is_ax_api_enabled == NULL) {
        return false;
    }

    get_prompt_user_if_ax_api_disabled = (get_prompt_user_if_ax_api_disabled_t) dlsym(backend_handle, "hook_get_prompt_user_if_ax_api_disabled");
    if (get_prompt_user_if_ax_api_disabled == NULL) {
        return false;
    }

    set_prompt_user_if_ax_api_disabled = (set_prompt_user_if_ax_api_disabled_t) dlsym(backend_handle, "hook_set_prompt_user_if_ax_api_disabled");
    if (set_prompt_user_if_ax_api_disabled == NULL) {
        return false;
    }

    get_ax_poll_frequency = (get_ax_poll_frequency_t) dlsym(backend_handle, "hook_get_ax_poll_frequency");
    if (get_ax_poll_frequency == NULL) {
        return false;
    }

    set_ax_poll_frequency = (set_ax_poll_frequency_t) dlsym(backend_handle, "hook_set_ax_poll_frequency");
    if (set_ax_poll_frequency == NULL) {
        return false;
    }

    get_post_text_delay_x11 = (get_post_text_delay_x11_t) dlsym(backend_handle, "hook_get_post_text_delay_x11");
    if (get_post_text_delay_x11 == NULL) {
        return false;
    }

    set_post_text_delay_x11 = (set_post_text_delay_x11_t) dlsym(backend_handle, "hook_set_post_text_delay_x11");
    if (set_post_text_delay_x11 == NULL) {
        return false;
    }

    create_screen_info = (create_screen_info_t) dlsym(backend_handle, "hook_create_screen_info");
    if (create_screen_info == NULL) {
        return false;
    }

    get_auto_repeat_rate = (get_auto_repeat_rate_t) dlsym(backend_handle, "hook_get_auto_repeat_rate");
    if (get_auto_repeat_rate == NULL) {
        return false;
    }

    get_auto_repeat_delay = (get_auto_repeat_delay_t) dlsym(backend_handle, "hook_get_auto_repeat_delay");
    if (get_auto_repeat_delay == NULL) {
        return false;
    }

    get_pointer_acceleration_multiplier = (get_pointer_acceleration_multiplier_t) dlsym(backend_handle, "hook_get_pointer_acceleration_multiplier");
    if (get_pointer_acceleration_multiplier == NULL) {
        return false;
    }

    get_pointer_acceleration_threshold = (get_pointer_acceleration_threshold_t) dlsym(backend_handle, "hook_get_pointer_acceleration_threshold");
    if (get_pointer_acceleration_threshold == NULL) {
        return false;
    }

    get_pointer_sensitivity = (get_pointer_sensitivity_t) dlsym(backend_handle, "hook_get_pointer_sensitivity");
    if (get_pointer_sensitivity == NULL) {
        return false;
    }

    get_multi_click_time = (get_multi_click_time_t) dlsym(backend_handle, "hook_get_multi_click_time");
    if (get_multi_click_time == NULL) {
        return false;
    }

    return true;
}

static bool open_backend() {
    const char* selected_backend_name = backend_name != NULL
        ? backend_name
        : get_backend_name();

    backend_handle = dlopen(selected_backend_name, RTLD_LAZY | RTLD_LOCAL);
    if (backend_handle == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open %s: %s!\n",
                __FUNCTION__, __LINE__, selected_backend_name, dlerror());

        return false;
    }

    if (!load_backend_symbols()) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to load symbols from %s: %s!\n",
                __FUNCTION__, __LINE__, selected_backend_name, dlerror());

        if (!dlclose(backend_handle)) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to close %s: %s!\n",
                    __FUNCTION__, __LINE__, selected_backend_name, dlerror());
        }

        backend_handle = NULL;
        return false;
    }

    return true;
}

static bool ensure_backend_loaded() {
    if (backend_handle != NULL) {
        return true;
    }

    return open_backend();
}
