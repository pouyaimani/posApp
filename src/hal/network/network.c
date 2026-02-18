#include "network.h"
#include "../dev/dev.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"

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

OOP_CTOR(Network) {

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