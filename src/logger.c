#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <uiohook.h>

#include "logger.h"

static logger_t callback = NULL;
static void *callback_data = NULL;

void logger(unsigned int level, const char *format, ...) {
    if (callback != NULL) {
        va_list args;

        va_start(args, format);
        callback(level, callback_data, format, args);
        va_end(args);
    }
}

void hook_set_logger_proc(logger_t logger_proc, void *user_data) {
    callback = logger_proc;
    callback_data = user_data;
}
