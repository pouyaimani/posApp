#ifndef WIFI_H_
#define WIFI_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum WifiSigStrenght_t {
    WIFI_SIGNAL_STRENGTH_0,
    WIFI_SIGNAL_STRENGTH_1,
    WIFI_SIGNAL_STRENGTH_2,
    WIFI_SIGNAL_STRENGTH_3
} WifiSigStrenght_t;

OOP_DECLARE_CLASS(Wifi)

OOP_VTABLE(Wifi) {
    OOP_IMETHOD(void, Wifi, init);
};

OOP_CLASS(Wifi) {
    OOP_IMPLEMENTS(Wifi);
    int dummy;
};

OOP_CTOR(Wifi);

Wifi *getWifi(void);

#endif