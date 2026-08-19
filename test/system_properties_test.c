#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <uiohook.h>

#include "minunit.h"

static bool pointer_properties_supported() {
    return hook_get_optional_feature_support() & UIOHOOK_FEATURE_POINTER_PROPERTIES;
}

static char * test_auto_repeat_rate() {
    long int i = hook_get_auto_repeat_rate();
    
    fprintf(stdout, "Auto repeat rate: %li\n", i);
    mu_assert("error, could not determine auto repeat rate", i >= 0);

    return NULL;
}

static char * test_auto_repeat_delay() {
    long int i = hook_get_auto_repeat_delay();
    
    fprintf(stdout, "Auto repeat delay: %li\n", i);
    mu_assert("error, could not determine auto repeat delay", i >= 0);

    return NULL;
}

static char * test_pointer_acceleration_multiplier() {
    long int i = hook_get_pointer_acceleration_multiplier();

    fprintf(stdout, "Pointer acceleration multiplier: %li\n", i);

    if (pointer_properties_supported()) {
        mu_assert("error, could not determine pointer acceleration multiplier", i >= 0);
    } else {
        mu_assert("error, the pointer acceleration multiplier is reported as available without support", i == -1);
    }

    return NULL;
}

static char * test_pointer_acceleration_threshold() {
    long int i = hook_get_pointer_acceleration_threshold();

    fprintf(stdout, "Pointer acceleration threshold: %li\n", i);

    if (pointer_properties_supported()) {
        mu_assert("error, could not determine pointer acceleration threshold", i >= 0);
    } else {
        mu_assert("error, the pointer acceleration threshold is reported as available without support", i == -1);
    }

    return NULL;
}

static char * test_pointer_sensitivity() {
    long int i = hook_get_pointer_sensitivity();

    fprintf(stdout, "Pointer sensitivity: %li\n", i);

    if (pointer_properties_supported()) {
        mu_assert("error, could not determine pointer sensitivity", i >= 0);
    } else {
        mu_assert("error, the pointer sensitivity is reported as available without support", i == -1);
    }

    return NULL;
}

static char * test_multi_click_time() {
    long int i = hook_get_multi_click_time();

    fprintf(stdout, "Multi click time: %li\n", i);
    mu_assert("error, could not determine multi click time", i >= 0);

    return NULL;
}

char * system_properties_tests() {
    if (!pointer_properties_supported()) {
        fprintf(stdout, "The pointer properties are not supported, so they are only checked for reporting -1.\n");
    }

    mu_run_test(test_auto_repeat_rate);
    mu_run_test(test_auto_repeat_delay);

    mu_run_test(test_pointer_acceleration_multiplier);
    mu_run_test(test_pointer_acceleration_threshold);
    mu_run_test(test_pointer_sensitivity);

    mu_run_test(test_multi_click_time);

    return NULL;
}
