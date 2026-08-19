#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uiohook.h"
#include "input_helper.h"
#include "minunit.h"

extern char * system_properties_tests();
extern char * input_helper_tests();

#ifdef __linux__
extern char * evdev_input_helper_tests();
#endif

int tests_run = 0;

#ifdef __linux__
#define REQUIRES_DISPLAY true
#else
#define REQUIRES_DISPLAY false
#endif

typedef struct _test_suite {
    const char *name;
    char * (*run)();
    bool requires_display;
} test_suite;

static const test_suite test_suites[] = {
    { "system_properties", system_properties_tests, REQUIRES_DISPLAY },
    { "input_helper", input_helper_tests, REQUIRES_DISPLAY },

    #ifdef __linux__
    { "evdev_input_helper", evdev_input_helper_tests, false },
    #endif
};

#define TEST_SUITE_COUNT (sizeof(test_suites) / sizeof(*test_suites))

static bool selected[TEST_SUITE_COUNT];

static bool input_helper_needed = false;

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
    #ifndef _WIN32
    if (input_helper_needed) {
        load_input_helper();
    }
    #endif
    return NULL;
}

static char * cleanup_tests() {
    #ifndef _WIN32
    if (input_helper_needed) {
        unload_input_helper();
    }
    #endif
    return NULL;
}

static char * run_selected_tests() {
    mu_run_test(init_tests);

    for (size_t i = 0; i < TEST_SUITE_COUNT; i++) {
        if (selected[i]) {
            printf("Running the %s test suite.\n", test_suites[i].name);
            mu_run_test(test_suites[i].run);
        }
    }

    mu_run_test(cleanup_tests);

    return NULL;
}

static void print_suites() {
    printf("Available test suites:\n");

    for (size_t i = 0; i < TEST_SUITE_COUNT; i++) {
        printf("  %-20s %s\n", test_suites[i].name,
                test_suites[i].requires_display ? "(requires a display)" : "(headless)");
    }
}

static void print_usage(const char *program) {
    printf("Usage: %s [--headless | --list | --help | <suite>...]\n\n", program);
    printf("  <suite>...   Run only the named suites. All suites run when none are named.\n");
    printf("  --headless   Run only the suites which don't need a display.\n");
    printf("  --list       List the available suites and exit.\n");
    printf("  --help       Print this message and exit.\n\n");

    print_suites();
}

static bool select_tests(int argc, char *argv[]) {
    if (argc <= 1) {
        for (size_t i = 0; i < TEST_SUITE_COUNT; i++) {
            selected[i] = true;
        }

        return true;
    }

    if (strcmp(argv[1], "--headless") == 0) {
        if (argc > 2) {
            printf("--headless cannot be combined with other arguments.\n");
            return false;
        }

        for (size_t i = 0; i < TEST_SUITE_COUNT; i++) {
            selected[i] = !test_suites[i].requires_display;
        }

        return true;
    }

    for (int arg = 1; arg < argc; arg++) {
        bool found = false;

        for (size_t i = 0; i < TEST_SUITE_COUNT; i++) {
            if (strcmp(argv[arg], test_suites[i].name) == 0) {
                selected[i] = true;
                found = true;
                break;
            }
        }

        if (!found) {
            printf("Unknown test suite: %s\n\n", argv[arg]);
            print_suites();

            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    hook_set_logger_proc(logger_proc, NULL);

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "--list") == 0)) {
        if (strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
        } else {
            print_suites();
        }

        return EXIT_SUCCESS;
    }

    if (!select_tests(argc, argv)) {
        return EXIT_FAILURE;
    }

    bool any_selected = false;

    for (size_t i = 0; i < TEST_SUITE_COUNT; i++) {
        if (selected[i]) {
            any_selected = true;

            if (test_suites[i].requires_display) {
                input_helper_needed = true;
            }
        }
    }

    if (!any_selected) {
        printf("No test suites were selected.\n\n");
        print_suites();

        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;

    char *result = run_selected_tests();

    if (result != NULL) {
        status = EXIT_FAILURE;
        printf("%s\n", result);
    } else {
        printf("ALL TESTS PASSED\n");
    }

    printf("Tests run: %d\n", tests_run);

    return status;
}
