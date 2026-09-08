#ifndef NTH_TRANSPORT_H
#define NTH_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "nth_types.h"

typedef struct {

    int (*connect)(const char* host, uint16_t port);

    int (*send)(int fd, const void* buf, size_t len);

    /*
     * bytesReceived is meaningful only for NTH_IO_DATA.
     */
    NthIoStatus (*recv)(int fd, void* buf, size_t capacity,
                        size_t* bytesReceived);

    int (*poll)(int fd, uint32_t timeoutMs);

    int (*close)(int fd);

} NthTransport;

#endif