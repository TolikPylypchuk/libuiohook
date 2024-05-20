#include <stdbool.h>
#include <Windows.h>

typedef struct {
    LONG left;
    LONG top;
} largest_negative_coordinates;

extern void enumerate_displays();

extern void set_always_enumerate_displays(bool always);

extern largest_negative_coordinates get_largest_negative_coordinates();
