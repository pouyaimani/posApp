#include "network.h"
#include "sys/sys.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"
#include "settings/settings.h"
#include "len.h"
#include "cellular/cellular.h"
#include "wifi/wifi.h"

Network* __network;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/network_t3Rtos.h"

static void constructT3Rtos() {
    static NetworkT3Rtos obj;
    __network = (Network*)&obj;
    OOP_CALL_CTOR(Network, __network);
    OOP_CALL_CTOR(NetworkT3Rtos, &obj);
}

#endif

static uint32_t tick;

static Result_t setRoute(NetRoute_t route) {
    Result_t res;
    res.err = ERR_DSC_OK;
    LOG_TRACE("Network: setting route to %d", route);
    NetError_t err = OOP_CALL(network(), setRoute, route);
    RETURN_VALUE_IF_NOT(err, NET_ERR_OK, res.err = ERR_DSC_DEVICE;, res);

    OOP_CALL(cellular(), close);
    OOP_CALL(wifi(), close);
    if (route == NET_ROUTE_CELLULAR) {
        if (OOP_CALL(cellular(), init) != CELL_ERR_OK) {
            res.err             = ERR_DSC_CELLULAR;
            res.detail.cellular = CELL_ERR_INIT;
        }
    } else if (route == NET_ROUTE_WIFI) {
        if (OOP_CALL(wifi(), init) != WIFI_ERR_OK) {
            res.err         = ERR_DSC_WIFI;
            res.detail.wifi = WIFI_ERR_INIT;
        }
    } else if (route == NET_ROUTE_ETH) {
    }
    LOG_TRACE("Network: setting route done. error =  %d", res.err);
    return res;
}

static Result_t init(NetRoute_t route) {
    Result_t res = setRoute(route);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, res);
    res.err = ERR_DSC_OK;
    return res;
}

static void checkSocketConnectStatus() {
    SocketStatus_t st = OOP_CALL(__network, getStatus, __network->id);
    if (st != NET_STATUS_CONNECTING) {
        SocketConnectEvent* ev =
            (WifiEvent*)createEvent(SM_EVENT_SOCKET_CONNECT);
        ev->isConnected = st == NET_STATUS_CONNECTED;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkSocketConnectStatus);
    }
    if (GET_TICK() - tick >= CONNECT_TIME_OUT) {
        SocketConnectEvent* ev =
            (WifiEvent*)createEvent(SM_EVENT_SOCKET_CONNECT);
        ev->isConnected = false;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkSocketConnectStatus);
    }
}

static int connect() {
    SocketAddr_t addr;
    addr.family = NET_AF_INET;
    snprintf(addr.ip, sizeof(addr.ip), "%s", settings()->server.mainServerIp);
    addr.port         = settings()->server.mainServerPort;
    SocketType_t type = NET_STREAM;
    __network->id     = OOP_CALL(__network, create, &addr, type);
    tick              = GET_TICK();
    getEventloop()->registerChecker(checkSocketConnectStatus);
    return __network->id;
}

static void checkSocketReceive() {
    DEFINE_BYTE_ARRAY(recBuffer, REC_BUFF_LEN);
    int ret =
        OOP_CALL(__network, receive, __network->id, recBuffer, REC_BUFF_LEN);
    SocketReadyReadEvent* ev =
        (SocketReadyReadEvent*)createEvent(SM_EVENT_SOCKET_READY_READ);
    ev->ba.data = ret > 0 ? recBuffer : NULL;
    ev->ba.len  = ret;
    DISPATCH_EVENT(ev);
    getEventloop()->unregisterChecker(checkSocketReceive);
}

static int send(uint8_t* data, size_t len) {
    int ret =
        OOP_CALL(__network, send, __network->id, data, len, SEND_TIME_OUT);
    if (ret == len) {
        getEventloop()->registerChecker(checkSocketReceive);
        tick = GET_TICK();
    }
    return ret;
}

static void disconnect() { OOP_CALL(__network, close, __network->id); }

OOP_CTOR(Network) {
    self->init       = init;
    self->connect    = connect;
    self->send       = send;
    self->disconnect = disconnect;
    self->setRoute   = setRoute;
}

Network* network() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        constructT3Rtos();
#else
#error Deivce network is undefined. Make sure correct device is chosen and its network driver is developed.
#endif
    );
    return __network;
}