#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <uiohook.h>

#ifndef __FUNCTION__
#define __FUNCTION__ __func__
#endif

extern void logger(unsigned int level, const char *format, ...);

#endif
