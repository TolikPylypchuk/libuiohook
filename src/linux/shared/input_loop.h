#ifndef INPUT_LOOP_H
#define INPUT_LOOP_H

#include <stdbool.h>

int run_libinput(bool keyboard, bool mouse);

int stop_libinput();

#endif
