#ifndef PICC_EVENT_H
#define PICC_EVENT_H

#include "event.h"

/* -----------------------------------------------------------
 * Detected card type
 * ----------------------------------------------------------- */

typedef enum {
    PICC_CARD_NONE = 0,

    PICC_CARD_A,
    PICC_CARD_B,

    PICC_CARD_A_MEMORY,
    PICC_CARD_B_MEMORY,

    PICC_CARD_FELICA,

    PICC_CARD_MULTIPLE,

    PICC_CARD_UNKNOWN
} PiccCardType_t;

/* -----------------------------------------------------------
 * Generic PICC state
 * ----------------------------------------------------------- */

typedef struct {
    PiccCardType_t cardType;
} PiccData_t;

OOP_CLASS(PiccEvent) {
    OOP_EXTENDS(Event);
    PiccData_t* data;
};

/* ctor */
OOP_CTOR(PiccEvent);

void piccEventRegister(void);

#endif