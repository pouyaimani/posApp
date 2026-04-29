#ifndef DEBUG_PRINT_H_
#define DEBUG_PRINT_H_

#include "logger.h"

#include <stdarg.h>

#define debug_log(format, ...) LOG_ERROR(format, ##__VA_ARGS__)

#endif  // DEBUG_PRINT_H_