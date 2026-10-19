#ifndef PICC_EVENT_H
#define PICC_EVENT_H

#include <stdint.h>
#include "event.h"

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

/** Detection metadata only; UID, PAN and APDU payloads are not available here.
 */
typedef struct {
    PiccCardType_t cardType;
} PiccData_t;

OOP_CLASS(PiccEvent) {
    OOP_EXTENDS(Event);
    PiccData_t*
        data; /**< Points to this event's snapshot, never live HAL data. */
    PiccData_t
        snapshot; /**< Owned by the event; valid until the event is released. */
    uint32_t scanId; /**< Scan generation that produced this event. */
    int error; /**< PiccErr_t value; int avoids circular header dependency. */
};

OOP_CTOR(PiccEvent);

/** Existing registration must allocate sizeof(PiccEvent) from this updated
 * header. */
void piccEventRegister(void);

#endif
