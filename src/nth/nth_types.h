#ifndef NT_TYPES_H
#define NT_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    NTH_OK = 0,
    NTH_ERR_INVALID_HOST,
    NTH_ERR_INVALID_ARG,
    NTH_ERR_NO_MEMORY,
    NTH_ERR_TIMEOUT,
    NTH_ERR_CONNECT,
    NTH_ERR_SEND,
    NTH_ERR_RECEIVE,
    NTH_ERR_DISCONNECTED,
    NTH_ERR_OVERFLOW,
    NTH_ERR_INTERNAL
} NthResult;

typedef enum {
    NTH_TX_IDLE = 0,
    NTH_TX_CONNECTING,
    NTH_TX_SENDING,
    NTH_TX_RECEIVING,
    NTH_TX_COMPLETED,
    NTH_TX_FAILED,
} NthTxState;

#endif