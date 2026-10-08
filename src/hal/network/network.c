#include "network.h"
#include "sys/sys.h"
#include "eventloop.h"
#include "eventAll.h"
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
    Result_t res = {.err = ERR_DSC_OK};
    LOG_TRACE("Network: setting route to %d", route);
    NetError_t err = OOP_CALL(network(), setRoute, route);
    RETURN_VALUE_IF_NOT(err, NET_ERR_OK, res.err = ERR_DSC_DEVICE;, res);
    return res;
}

static Result_t init(NetRoute_t route) {
    Result_t res = {.err = ERR_DSC_OK};
    if (route == NET_ROUTE_CELLULAR) {
        OOP_CALL(wifi(), close);
        if (OOP_CALL(cellular(), init) != CELL_ERR_OK) {
            res.err             = ERR_DSC_CELLULAR;
            res.detail.cellular = CELL_ERR_INIT;
        }
    } else if (route == NET_ROUTE_WIFI) {
        OOP_CALL(cellular(), close);
        if (OOP_CALL(wifi(), init) != WIFI_ERR_OK) {
            res.err         = ERR_DSC_WIFI;
            res.detail.wifi = WIFI_ERR_INIT;
        }
    } else if (route == NET_ROUTE_ETH) {
    }
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, res);
    return setRoute(route);
}

OOP_CTOR(Network) {
    self->init     = init;
    self->setRoute = setRoute;
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