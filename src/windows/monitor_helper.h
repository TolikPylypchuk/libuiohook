#include <Windows.h>

typedef struct {
    LONG left;
    LONG top;
} largest_negative_coordinates;

extern void enumerate_displays();

extern largest_negative_coordinates get_largest_negative_coordinates();
