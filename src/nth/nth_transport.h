#ifndef NT_TRANSPORT_H
#define NT_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {

    int (*connect)(const char* host, uint16_t port);

    int (*send)(int fd, const void* buf, size_t len);

    int (*recv)(int fd, void* buf, size_t len);

    int (*poll)(int fd, uint32_t timeoutMs);

    int (*close)(int fd);

} NthTransport;

#endif