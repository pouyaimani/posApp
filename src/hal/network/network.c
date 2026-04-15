#include "network.h"
#include "../dev/dev.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"
#include "storage/storage.h"
#include "settings/settings.h"

Network *__network;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/network_t3Rtos.h"

static void constructT3Rtos() {
    static NetworkT3Rtos obj;
    __network = (Network *)&obj;
    OOP_CALL_CTOR(Network, __network);
    OOP_CALL_CTOR(NetworkT3Rtos, &obj);
}

#endif

#define CONNECT_TIME_OUT 20000
#define SEND_TIME_OUT 20000
#define REC_TIME_OUT 20000
#define REC_BUFF_LEN 1024

static int socketId;
static uint32_t tick;
static uint8_t *recBuffer;

static void checkSocketConnectStatus() {
    SocketStatus_t st = OOP_CALL(__network, getStatus, socketId);
    if (st != NET_STATUS_CONNECTING) {
        SocketConnectEvent *ev = (WifiEvent*)createEvent(SM_EVENT_SOCKET_CONNECT);
        ev->isConnected = st == NET_STATUS_CONNECTED;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkSocketConnectStatus);
    }
    if (GET_TICK() - tick >= CONNECT_TIME_OUT) {
        SocketConnectEvent *ev = (WifiEvent*)createEvent(SM_EVENT_SOCKET_CONNECT);
        ev->isConnected = false;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkSocketConnectStatus);
    }
}

static int connect() {
    SocketAddr_t addr;
    addr.family = NET_AF_INET;
    snprintf(addr.ip, 
        sizeof(addr.ip), "%s", settings()->server.mainServerIp);
    addr.port = settings()->server.mainServerPort;
    SocketType_t type = NET_STREAM;
    socketId = OOP_CALL(__network, create, &addr, type);
    tick = GET_TICK();
    getEventloop()->registerChecker(checkSocketConnectStatus);
    return socketId;
}

static void checkSocketReceive() {
    int ret = OOP_CALL(__network, receive, socketId, recBuffer, REC_BUFF_LEN);
    if (ret > 0) {
        SocketReadyReadEvent *ev = (WifiEvent*)createEvent(SM_EVENT_SOCKET_READY_READ);
        ev->recData = recBuffer;
        ev->recDataLen = ret;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkSocketReceive);
    }
    if (GET_TICK() - tick >= REC_TIME_OUT) {
        SocketReadyReadEvent *ev = (WifiEvent*)createEvent(SM_EVENT_SOCKET_READY_READ);
        ev->recData = NULL;
        ev->recDataLen = -1;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkSocketReceive);
    }
}

static int send(uint8_t *data, size_t len) {
    int ret = OOP_CALL(__network, send, socketId, data, len, SEND_TIME_OUT);
    if (ret == len) {
        getEventloop()->registerChecker(checkSocketReceive);
        tick = GET_TICK();
    }
    return ret;
}

OOP_CTOR(Network) {
    self->connect = connect;
    self->send = send;
    recBuffer = GET_MEM(REC_BUFF_LEN);
}

Network *getNetwork() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce network is undefined. Make sure correct device is chosen and its network driver is developed.
#endif
    );
    return __network;
}