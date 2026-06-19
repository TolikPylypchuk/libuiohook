#define _GNU_SOURCE

#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

static bool backend_loaded = false;
static const char *backend_name = NULL;
static pthread_mutex_t backend_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char const * BACKEND_X11_NAME = "x11";
static const char const * BACKEND_WAYLAND_NAME = "wayland";
static const char const * BACKEND_LEGACY_NAME = "legacy";

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

static bool load_backend();

void logger(unsigned int level, const char *format, ...) {
    if (callback != NULL) {
        va_list args;

        va_start(args, format);
        callback(level, callback_data, format, args);
        va_end(args);
    }
}

int hook_get_linux_backend() {
    int result = LINUX_BACKEND_AUTO;

    pthread_mutex_lock(&backend_mutex);

    if (backend_name == NULL) {
        result = LINUX_BACKEND_AUTO;
    } else if (strcmp(backend_name, BACKEND_X11_NAME) == 0) {
        result = LINUX_BACKEND_X11;
    } else if (strcmp(backend_name, BACKEND_WAYLAND_NAME) == 0) {
        result = LINUX_BACKEND_WAYLAND;
    } else if (strcmp(backend_name, BACKEND_LEGACY_NAME) == 0) {
        result = LINUX_BACKEND_LEGACY;
    } else {
        result = LINUX_BACKEND_AUTO;
    }

    pthread_mutex_unlock(&backend_mutex);

    return result;
}

bool hook_set_linux_backend(int backend) {
    pthread_mutex_lock(&backend_mutex);

    if (backend_loaded) {
        pthread_mutex_unlock(&backend_mutex);
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

    pthread_mutex_unlock(&backend_mutex);
    return true;
}

void hook_set_logger_proc(logger_t logger_proc, void *user_data) {
    callback = logger_proc;
    callback_data = user_data;

    if (!load_backend()) {
        return;
    }

    set_logger_proc(logger_proc, user_data);
}

void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    if (!load_backend()) {
        return;
    }

    set_dispatch_proc(dispatch_proc, user_data);
}

int hook_run() {
    if (!load_backend()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return run();
}

int hook_run_keyboard() {
    if (!load_backend()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return run_keyboard();
}

int hook_run_mouse() {
    if (!load_backend()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return run_mouse();
}

int hook_stop() {
    if (!load_backend()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return stop();
}

int hook_post_event(uiohook_event * const event) {
    if (!load_backend()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return post_event(event);
}

int hook_post_events(uiohook_event * const events, uint32_t size) {
    if (!load_backend()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return post_events(events, size);
}

int hook_post_text(const uint16_t * const text) {
    if (!load_backend()) {
        return UIOHOOK_ERROR_LOAD_LINUX_BACKEND;
    }

    return post_text(text);
}

bool hook_is_key_typed_enabled() {
    if (!load_backend()) {
        return false;
    }

    return is_key_typed_enabled();
}

void hook_set_key_typed_enabled(bool enabled) {
    if (!load_backend()) {
        return;
    }

    set_key_typed_enabled(enabled);
}

bool hook_is_ax_api_enabled(bool promptUserIfDisabled) {
    if (!load_backend()) {
        return false;
    }

    return is_ax_api_enabled(promptUserIfDisabled);
}

bool hook_get_prompt_user_if_ax_api_disabled() {
    if (!load_backend()) {
        return false;
    }

    return get_prompt_user_if_ax_api_disabled();
}

void hook_set_prompt_user_if_ax_api_disabled(bool promptUserIfDisabled) {
    if (!load_backend()) {
        return;
    }

    set_prompt_user_if_ax_api_disabled(promptUserIfDisabled);
}

uint32_t hook_get_ax_poll_frequency() {
    if (!load_backend()) {
        return 0;
    }

    return get_ax_poll_frequency();
}

void hook_set_ax_poll_frequency(uint32_t frequency) {
    if (!load_backend()) {
        return;
    }

    set_ax_poll_frequency(frequency);
}

uint64_t hook_get_post_text_delay_x11() {
    if (!load_backend()) {
        return 0;
    }

    return get_post_text_delay_x11();
}

void hook_set_post_text_delay_x11(uint64_t delay) {
    if (!load_backend()) {
        return;
    }

    set_post_text_delay_x11(delay);
}

screen_data* hook_create_screen_info(unsigned char *count) {
    if (!load_backend()) {
        return NULL;
    }

    return create_screen_info(count);
}

long int hook_get_auto_repeat_rate() {
    if (!load_backend()) {
        return -1;
    }

    return get_auto_repeat_rate();
}

long int hook_get_auto_repeat_delay() {
    if (!load_backend()) {
        return -1;
    }

    return get_auto_repeat_delay();
}

long int hook_get_pointer_acceleration_multiplier() {
    if (!load_backend()) {
        return -1;
    }

    return get_pointer_acceleration_multiplier();
}

long int hook_get_pointer_acceleration_threshold() {
    if (!load_backend()) {
        return -1;
    }

    return get_pointer_acceleration_threshold();
}

long int hook_get_pointer_sensitivity() {
    if (!load_backend()) {
        return -1;
    }

    return get_pointer_sensitivity();
}

long int hook_get_multi_click_time() {
    if (!load_backend()) {
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

static bool load_backend_symbols(void *handle) {
    set_logger_proc = (set_logger_proc_t) dlsym(handle, "hook_set_logger_proc");
    if (set_logger_proc == NULL) {
        return false;
    }

    set_dispatch_proc = (set_dispatch_proc_t) dlsym(handle, "hook_set_dispatch_proc");
    if (set_dispatch_proc == NULL) {
        return false;
    }

    run = (run_t) dlsym(handle, "hook_run");
    if (run == NULL) {
        return false;
    }

    run_keyboard = (run_keyboard_t) dlsym(handle, "hook_run_keyboard");
    if (run_keyboard == NULL) {
        return false;
    }

    run_mouse = (run_mouse_t) dlsym(handle, "hook_run_mouse");
    if (run_mouse == NULL) {
        return false;
    }

    stop = (stop_t) dlsym(handle, "hook_stop");
    if (stop == NULL) {
        return false;
    }

    post_event = (post_event_t) dlsym(handle, "hook_post_event");
    if (post_event == NULL) {
        return false;
    }

    post_events = (post_events_t) dlsym(handle, "hook_post_events");
    if (post_events == NULL) {
        return false;
    }

    post_text = (post_text_t) dlsym(handle, "hook_post_text");
    if (post_text == NULL) {
        return false;
    }

    is_key_typed_enabled = (is_key_typed_enabled_t) dlsym(handle, "hook_is_key_typed_enabled");
    if (is_key_typed_enabled == NULL) {
        return false;
    }

    set_key_typed_enabled = (set_key_typed_enabled_t) dlsym(handle, "hook_set_key_typed_enabled");
    if (set_key_typed_enabled == NULL) {
        return false;
    }

    is_ax_api_enabled = (is_ax_api_enabled_t) dlsym(handle, "hook_is_ax_api_enabled");
    if (is_ax_api_enabled == NULL) {
        return false;
    }

    get_prompt_user_if_ax_api_disabled = (get_prompt_user_if_ax_api_disabled_t) dlsym(handle, "hook_get_prompt_user_if_ax_api_disabled");
    if (get_prompt_user_if_ax_api_disabled == NULL) {
        return false;
    }

    set_prompt_user_if_ax_api_disabled = (set_prompt_user_if_ax_api_disabled_t) dlsym(handle, "hook_set_prompt_user_if_ax_api_disabled");
    if (set_prompt_user_if_ax_api_disabled == NULL) {
        return false;
    }

    get_ax_poll_frequency = (get_ax_poll_frequency_t) dlsym(handle, "hook_get_ax_poll_frequency");
    if (get_ax_poll_frequency == NULL) {
        return false;
    }

    set_ax_poll_frequency = (set_ax_poll_frequency_t) dlsym(handle, "hook_set_ax_poll_frequency");
    if (set_ax_poll_frequency == NULL) {
        return false;
    }

    get_post_text_delay_x11 = (get_post_text_delay_x11_t) dlsym(handle, "hook_get_post_text_delay_x11");
    if (get_post_text_delay_x11 == NULL) {
        return false;
    }

    set_post_text_delay_x11 = (set_post_text_delay_x11_t) dlsym(handle, "hook_set_post_text_delay_x11");
    if (set_post_text_delay_x11 == NULL) {
        return false;
    }

    create_screen_info = (create_screen_info_t) dlsym(handle, "hook_create_screen_info");
    if (create_screen_info == NULL) {
        return false;
    }

    get_auto_repeat_rate = (get_auto_repeat_rate_t) dlsym(handle, "hook_get_auto_repeat_rate");
    if (get_auto_repeat_rate == NULL) {
        return false;
    }

    get_auto_repeat_delay = (get_auto_repeat_delay_t) dlsym(handle, "hook_get_auto_repeat_delay");
    if (get_auto_repeat_delay == NULL) {
        return false;
    }

    get_pointer_acceleration_multiplier = (get_pointer_acceleration_multiplier_t) dlsym(handle, "hook_get_pointer_acceleration_multiplier");
    if (get_pointer_acceleration_multiplier == NULL) {
        return false;
    }

    get_pointer_acceleration_threshold = (get_pointer_acceleration_threshold_t) dlsym(handle, "hook_get_pointer_acceleration_threshold");
    if (get_pointer_acceleration_threshold == NULL) {
        return false;
    }

    get_pointer_sensitivity = (get_pointer_sensitivity_t) dlsym(handle, "hook_get_pointer_sensitivity");
    if (get_pointer_sensitivity == NULL) {
        return false;
    }

    get_multi_click_time = (get_multi_click_time_t) dlsym(handle, "hook_get_multi_click_time");
    if (get_multi_click_time == NULL) {
        return false;
    }

    return true;
}

static bool load_backend() {
    if (backend_loaded) {
        return true;
    }

    pthread_mutex_lock(&backend_mutex);

    if (backend_loaded) {
        pthread_mutex_unlock(&backend_mutex);
        return true;
    }

    const char* selected_backend_name = backend_name != NULL
        ? backend_name
        : get_backend_name();

    Dl_info info;
    if (dladdr((void *) load_backend, &info) == 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to get the path of the current shared object: %s!\n",
                __FUNCTION__, __LINE__, dlerror());

        pthread_mutex_unlock(&backend_mutex);
        return false;
    }

    char dir[PATH_MAX];
    strncpy(dir, info.dli_fname, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    char *backend_directory = dirname(dir);

    char backend_path[PATH_MAX];
    snprintf(backend_path, sizeof(backend_path), "%s/libuiohook-%s.so", backend_directory, selected_backend_name);

    void *backend_handle = dlopen(backend_path, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
    if (backend_handle == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open backend '%s': %s!\n",
                __FUNCTION__, __LINE__, selected_backend_name, dlerror());

        pthread_mutex_unlock(&backend_mutex);
        return false;
    }

    if (!load_backend_symbols(backend_handle)) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to load symbols from backend '%s': %s!\n",
                __FUNCTION__, __LINE__, selected_backend_name, dlerror());

        pthread_mutex_unlock(&backend_mutex);
        return false;
    }

    backend_loaded = true;
    pthread_mutex_unlock(&backend_mutex);

    return true;
}
