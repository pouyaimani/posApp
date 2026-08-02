#ifdef DEVICE_TRENDITT3RTOS

#include "network_t3Rtos.h"
#include "posplatform.h"
#include "sdkNet.h"
#include "sys/sys.h"
#include "logger.h"

static NetError_t init(Network* self) {}

static NetRoute_t getRoute(Network* self) {
    SOCKET_ROUTE route = sdkNetGetRoute();
    switch (route) {
    case SOCKET_ROUTE_CELLULAR:
        return NET_ROUTE_CELLULAR;
    case SOCKET_ROUTE_WIFI:
        return NET_ROUTE_WIFI;
    case SOCKET_ROUTE_ETH:
        return NET_ROUTE_ETH;
    }
}

static NetError_t setRoute(Network* self, NetRoute_t route) {
    SOCKET_ROUTE sdkRoute;
    switch (route) {
    case NET_ROUTE_CELLULAR:
        sdkRoute = SOCKET_ROUTE_CELLULAR;
        break;
    case NET_ROUTE_WIFI:
        sdkRoute = SOCKET_ROUTE_WIFI;
        break;
    case NET_ROUTE_ETH:
        sdkRoute = SOCKET_ROUTE_CELLULAR;
        break;
    default:
        return NET_ERR_INPUT_ERR;
    }
    int ret = sdkNetSetRoute(sdkRoute);
    return ret == SDK_NET_OK ? NET_ERR_OK : NET_ERR_INPUT_ERR;
}

static void setAddr(Network* self, const char* ip, uint16_t port) {
    SockAddr addr;
    int      ret = sdkNetAddrSet(&addr, ip, port);
    return ret == SDK_NET_OK ? NET_ERR_OK : NET_ERR_INPUT_ERR;
}

static const SocketAddr_t* getAddr(Network* self) {
    SockAddr addr;
    int ret = sdkNetAddrGet(&addr, self->address.ip, 64, self->address.port);
    return ret == SDK_NET_OK ? NET_ERR_OK : NET_ERR_INPUT_ERR;
}

static int32_t create(Network* self, SocketAddr_t* addr, SocketType_t type) {
    SDK_SOCKET_TYPE sdkst;
    switch (type) {
    case NET_STREAM:
        sdkst = SOCKET_STREAM;
        break;
    case NET_DGRAM:
        sdkst = SOCKET_DGRAM;
        break;
    case NET_SEQPACKET:
        sdkst = SOCKET_SEQPACKET;
        break;
    case NET_RAW:
        sdkst = SOCKET_RAW;
        break;
    }
    SockAddr sadrr;
    if (sdkNetAddrSet(&sadrr, addr->ip, addr->port) != SDK_NET_OK) {
        LOG_ERROR(
            "T3Rtos: setting socket address is failed. ip = %s, port = %d",
            addr->ip, addr->port);
        return -1;
    }
    LOG_TRACE("T3Rtos: creating socket ...");
    int32_t fd = sdkNetCreateSocket(&sadrr, sdkst);
    LOG_TRACE("T3Rtos: fd = %d", fd);
    return fd;
}

static NetError_t close(Network* self, int32_t id) {
    int ret = sdkNetCloseSocket(id);
    return ret == SDK_NET_OK ? NET_ERR_OK : NET_ERR_INPUT_ERR;
}

static SocketStatus_t getStatus(Network* self, int32_t id) {
    SOCKET_STATUS st = sdkNetGetSocketStatus(id);
    switch (st) {
    case SOCKET_STATUS_CONNECTING:
        return NET_STATUS_CONNECTING;
    case SOCKET_STATUS_CONNECTED:
        return NET_STATUS_CONNECTED;
    case SOCKET_STATUS_DISCONNECT:
        return NET_STATUS_DISCONNECTED;
    default:
        return SOCKET_STATUS_ERR;
    }
}

static int32_t send(Network* self, int32_t socketID, const uint8_t* data,
                    uint32_t dataLen, uint32_t timeOut) {
    return sdkNetSocketSend(socketID, data, dataLen, timeOut);
}

static int32_t receive(Network* self, int32_t socketID, const uint8_t* data,
                       uint32_t dataLen) {
    return sdkNetSocketRecv(socketID, data, dataLen);
}

OOP_CTOR(NetworkT3Rtos) {
    self->base.vtable.init      = init;
    self->base.vtable.close     = close;
    self->base.vtable.create    = create;
    self->base.vtable.getAddr   = getAddr;
    self->base.vtable.getRoute  = getRoute;
    self->base.vtable.getStatus = getStatus;
    self->base.vtable.receive   = receive;
    self->base.vtable.send      = send;
    self->base.vtable.setAddr   = setAddr;
    self->base.vtable.setRoute  = setRoute;
}

#endif
