#ifndef MAG_EVENT_H
#define MAG_EVENT_H

#include "eventAll.h"

typedef struct TrackData_t {
    uint16_t len;
    char*    data;
} TrackData_t;

typedef struct MagReaderData_t {
    TrackData_t track1;
    TrackData_t track2;
    TrackData_t track3;
} MagReaderData_t;

OOP_CLASS(MagEvent) {
    OOP_EXTENDS(Event);
    MagReaderData_t* data;
};

/* ctor */
OOP_CTOR(MagEvent);

void magEventRegister(void);

#endif