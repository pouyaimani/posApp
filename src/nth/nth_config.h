#ifndef NT_CONFIG_H
#define NT_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "sys/sys.h"
#include "logger.h"

#define NT_MAX_TRANSACTIONS 1
#define NT_MAX_EVENTS       32
#define NT_TX_BUFFER_SIZE   2048U
#define NT_RX_BUFFER_SIZE   6144U

#define NT_IO_BUFFER_SIZE 6144U

/*
 * Compatibility for older TX-oriented code.
 * New code should use the explicit names.
 */
#define NT_BUFFER_SIZE NT_TX_BUFFER_SIZE

#define NT_DEFAULT_TIMEOUT_MS 30000

#define NTH_GET_TICK() GET_TICK()

#define NTH_USE_LOG 1

#ifdef NTH_USE_LOG
#define NTH_LOG(...) LOG_TRACE(__VA_ARGS__)
#else
#define NTH_LOG(log)                                                           \
    do {                                                                       \
    } while (0)
#endif

#endif