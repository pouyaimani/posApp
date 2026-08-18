#ifndef CELLULAR_H_
#define CELLULAR_H_

#include "oop.h"
#include <stdint.h>
#include "event.h"
#include "error.h"

typedef enum CellSigStrength_t {
    CELL_SIGNAL_STRENGTH_0,
    CELL_SIGNAL_STRENGTH_1,
    CELL_SIGNAL_STRENGTH_2,
    CELL_SIGNAL_STRENGTH_3,
    CELL_SIGNAL_STRENGTH_INVALID
} CellSigStrength_t;

typedef enum CellNeyType_t {
    CELL_NET_TYPE_2G,
    CELL_NET_TYPE_3G,
    CELL_NET_TYPE_4G,
    CELL_NET_TYPE_INVALID
} CellNeyType_t;

typedef enum SimStatus_t {
    SIM_STATUS_ERR,
    SIM_STATUS_OK,
} SimStatus_t;

OOP_CLASS(CellSimInfo) {
    char iccId[32 + 1];
    char imsi[32 + 1];
};

OOP_DECLARE_CLASS(Cellular)

OOP_VTABLE(Cellular) {
    OOP_IMETHOD(CellErr_t, Cellular, init);
    OOP_IMETHOD(CellErr_t, Cellular, open);
    OOP_IMETHOD(CellErr_t, Cellular, close);
    OOP_IMETHOD(CellSigStrength_t, Cellular, getSignalStrength);
    OOP_IMETHOD(CellPPPStatus_t, Cellular, getPPPstatus);
    OOP_IMETHOD(CellErr_t, Cellular, startPPPlogin, const char*, const char*,
                const char*, const char*);
    OOP_IMETHOD(CellErr_t, Cellular, ussdInit);
    OOP_IMETHOD(CellErr_t, Cellular, ussdSend, char*, uint8_t);
    OOP_IMETHOD(CellErr_t, Cellular, ussdRec, char*, size_t);
    OOP_IMETHOD(CellErr_t, Cellular, ussdStop);
    OOP_IMETHOD(CellErr_t, Cellular, ussdGetCharset, char*, uint32_t);
    OOP_IMETHOD(CellErr_t, Cellular, ussdSetCharset, char*);
    OOP_IMETHOD(CellNeyType_t, Cellular, getNetType);
    OOP_IMETHOD(CellErr_t, Cellular, getSimInfo, CellSimInfo*);
    OOP_IMETHOD(CellErr_t, Cellular, selectSim, uint8_t);
    OOP_IMETHOD(SimStatus_t, Cellular, getSimStatus);
    OOP_IMETHOD(void, Cellular, checkSimStatus);
    OOP_IMETHOD(int8_t, Cellular, getImei, char*, size_t size);
};

OOP_CLASS(Cellular) {
    OOP_IMPLEMENTS(Cellular);
    OOP_METHOD(CellErr_t, startPPPlogin, const char*, const char*, const char*,
               const char*);
    CellSimInfo simInfo;
};

OOP_CTOR(Cellular);

Cellular* cellular();

#endif
