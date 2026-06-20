#ifndef _included_logger
#define _included_logger

#include <uiohook.h>
#include <stdbool.h>

#ifndef __FUNCTION__
#define __FUNCTION__ __func__
#endif

extern void logger(unsigned int level, const char *format, ...);

#endif
