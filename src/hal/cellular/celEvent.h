#ifndef CEL_EVENT_H
#define CEL_EVENT_H

#include "event.h"

/* ===== Cellullar ===== */

typedef enum CellPPPStatus_t {
    CELL_PPP_INIT,
    CELL_PPP_DIALING,
    CELL_PPP_READY,
    CELL_PPP_SUCESS,
    CELL_PPP_FAILURE,
    CELL_PPP_INVALID
} CellPPPStatus_t;

OOP_CLASS(CellEvent) {
    OOP_EXTENDS(Event);
    CellPPPStatus_t pppSt;
};

/* ctor */
OOP_CTOR(CellEvent);

void celEventRegister(void);

#endif