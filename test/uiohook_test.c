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
