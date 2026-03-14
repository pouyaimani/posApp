#ifndef EVENT_H
#define EVENT_H

#include "oop.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum SmEventType_t {
    SM_EVENT_TIME_OUT = 0,
    SM_EVENT_KEYPAD,
    SM_EVENT_MAG,
    SM_EVENT_WIFI,
    SM_EVENT_CELLULAR,
    SM_EVENT_SOCKET_CONNECT,
    SM_EVENT_SOCKET_SENT,
    SM_EVENT_SOCKET_READY_READ
} SmEventType_t;

/* Forward declarations */
OOP_DECLARE_CLASS(State);
OOP_DECLARE_CLASS(Event);
OOP_DECLARE_CLASS(TimeOutEvent);
OOP_DECLARE_CLASS(KeypadEvent);
OOP_DECLARE_CLASS(MagEvent);
OOP_DECLARE_CLASS(WifiEvent);
OOP_DECLARE_CLASS(CellEvent);
OOP_DECLARE_CLASS(SocketEvent);

/* ===== Event vtable ===== */

OOP_VTABLE(Event)
{
    OOP_IMETHOD(void, Event, dispatch, State *state);
    OOP_IMETHOD(void, Event, dispatchTo, State *state);
};

/* ===== Event base ===== */

OOP_CLASS(Event)
{
    OOP_IMPLEMENTS(Event);
    State *target;
};

/* Base API */
OOP_CTOR(Event);

Event *createEvent(SmEventType_t type);

#define DISPATCH_EVENT(event) \
    ((event)->base.vtable.dispatch((event), NULL));

/* ===== TimeOut ===== */

OOP_CLASS(TimeOutEvent)
{
    OOP_EXTENDS(Event);
};

/* ctor */
OOP_CTOR(TimeOutEvent);

/* ===== Keypad ===== */

typedef enum Key_t {
    KEY_NONE = -1,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_ENTER,
    KEY_ESC,
    KEY_UP,
    KEY_DOWN,
    KEY_CLEAR,
    KEY_ALPHA,
    KEY_FUNCTION,
    KEY_POWER
} Key_t;

OOP_CLASS(KeypadEvent)
{
    OOP_EXTENDS(Event);
    Key_t key;
    char keyStr;
};

/* ctor */
OOP_CTOR(KeypadEvent);

/* ===== Magreader ===== */

typedef struct TrackData_t {
    uint16_t len;
    char *data;
} TrackData_t;

typedef struct MagReaderData_t {
    TrackData_t track1;
    TrackData_t track2;
    TrackData_t track3;
} MagReaderData_t;

OOP_CLASS(MagEvent)
{
    OOP_EXTENDS(Event);
    MagReaderData_t *data;
};

/* ctor */
OOP_CTOR(MagEvent);

/* ===== Wifi ===== */

typedef enum WifiScanSt_t {
    WIFI_SCAN_UNDER_PROCESS,
    WIFI_SCAN_SUCCEED,
    WIFI_SCAN_FAILED
} WifiScanSt_t;

typedef enum WifiConnectSt_t {
    WIFI_CONNECT_UNDER_PROCESS,
    WIFI_CONNECT_SUCCEED,
    WIFI_CONNECT_FAILED
} WifiConnectSt_t;

OOP_DECLARE_CLASS(WifiApList_t);
  
OOP_CLASS(WifiEvent)
{
    OOP_EXTENDS(Event);
    WifiScanSt_t scanStatus;
    WifiConnectSt_t connectStatus;
    WifiApList_t *apList;
};

/* ctor */
OOP_CTOR(WifiEvent);

/* ===== Cellullar ===== */

typedef enum CellPPPStatus_t {
    CELL_PPP_DIALING,
    CELL_PPP_READY,
    CELL_PPP_SUCESS,
    CELL_PPP_FAILURE,
    CELL_PPP_INVALID
} CellPPPStatus_t;
  
OOP_CLASS(CellEvent)
{
    OOP_EXTENDS(Event);
    CellPPPStatus_t pppSt;
};

/* ctor */
OOP_CTOR(CellEvent);

/* ===== Socket ===== */

OOP_CLASS(SocketConnectEvent) {
    OOP_EXTENDS(Event);
    bool isConnected;
};
/* ctor */
OOP_CTOR(SocketConnectEvent);

OOP_CLASS(SocketSentEvent) {
    OOP_EXTENDS(Event);
    int sendErr;
};
/* ctor */
OOP_CTOR(SocketSentEvent);

OOP_CLASS(SocketReadyReadEvent) {
    OOP_EXTENDS(Event);
    uint8_t *recData;
    size_t recDataLen;
};
/* ctor */
OOP_CTOR(SocketReadyReadEvent);


#endif
