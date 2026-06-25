#include "nth_transport.h"
#include "nth_config.h"

#include <errno.h>
#include <string.h>
#include "network/network.h"
#include "error.h"

static int sys_sock_connect(const char* host, uint16_t port) {
    RETURN_VALUE_IF_NULL(host, ;, ERR_NULL_PARAMETER);
    SocketAddr_t addr;
    addr.family = NET_AF_INET;
    snprintf(addr.ip, sizeof(addr.ip), "%s", host);
    addr.port         = port;
    SocketType_t type = NET_STREAM;
    NTH_LOG("host ip = %s", addr.ip);
    NTH_LOG("host port = %d", addr.port);
    return OOP_CALL(network(), create, &addr, type);
}

static int sys_sock_send(int fd, const void* buf, size_t len) {
    return OOP_CALL(network(), send, fd, buf, len, NT_DEFAULT_TIMEOUT_MS);
}

static int sys_sock_recv(int fd, void* buf, size_t len) {
    return OOP_CALL(network(), receive, fd, buf, len);
}

static int sys_sock_poll(int fd, uint32_t timeoutMs) {
    SocketStatus_t st = OOP_CALL(network(), getStatus, fd);
    if (st != NET_STATUS_CONNECTING) {
        NTH_LOG("socket connect status = %d", st);
        return st == NET_STATUS_CONNECTED ? 1 : -1;
    }
    return 0;
}

static int sys_sock_close(int fd) {
    int            ret;
    SocketStatus_t st = OOP_CALL(network(), getStatus, fd);
    while (st == NET_STATUS_CONNECTED) {
        ret = OOP_CALL(network(), close, fd);
        st  = OOP_CALL(network(), getStatus, fd);
    }
    NTH_LOG("socket connect status = %d", st);
    return ret;
}

NthTransport sysTransport = {.connect = sys_sock_connect,
                             .send    = sys_sock_send,
                             .recv    = sys_sock_recv,
                             .poll    = sys_sock_poll,
                             .close   = sys_sock_close};