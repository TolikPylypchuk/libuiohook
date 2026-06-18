#include <stdbool.h>
#include <stdint.h>

#include <logger.h>
#include <uiohook.h>

static dispatcher_t dispatch = NULL;
static void *dispatch_data = NULL;

static bool key_typed_enabled = true;

void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting new dispatch callback to %#p.\n",
            __FUNCTION__, __LINE__, dispatch_proc);

    dispatch = dispatch_proc;
    dispatch_data = user_data;
}

bool hook_is_key_typed_enabled() {
    return key_typed_enabled;
}

void hook_set_key_typed_enabled(bool enabled) {
    key_typed_enabled = enabled;
}
