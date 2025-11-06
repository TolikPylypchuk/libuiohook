/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
 * https://github.com/kwhat/libuiohook/
 *
 * libUIOHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libUIOHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "uiohook.h"
#include "input_helper.h"
#include "minunit.h"

extern char * system_properties_tests();
extern char * input_helper_tests();

int tests_run = 0;

static char *map_log_level_name(unsigned int level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:
            return "DBG";
        case LOG_LEVEL_INFO:
            return "INF";
        case LOG_LEVEL_WARN:
            return "WRN";
        case LOG_LEVEL_ERROR:
            return "ERR";
        default:
            return "   ";
    };
}

static void logger_proc(unsigned int level, void *user_data, const char *format, va_list args) {
    printf("[%s] ", map_log_level_name(level));
    vfprintf(stdout, format, args);
}

static char * init_tests() {
    hook_set_logger_proc(logger_proc, NULL);

    #ifndef _WIN32
    load_input_helper();
    #endif
    return NULL;
}

static char * cleanup_tests() {
    #ifndef _WIN32
    unload_input_helper();
    #endif
    return NULL;
}

static char * all_tests() {
    mu_run_test(init_tests);

    mu_run_test(system_properties_tests);
    mu_run_test(input_helper_tests);

    mu_run_test(cleanup_tests);

    return NULL;
}

int main() {
    int status = EXIT_SUCCESS;

    char *result = all_tests();

    if (result != NULL) {
        status = EXIT_FAILURE;
        printf("%s\n", result);
    } else {
        printf("ALL TESTS PASSED\n");
    }

    printf("Tests run: %d\n", tests_run);

    return status;
}
