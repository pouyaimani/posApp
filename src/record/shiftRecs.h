#ifndef SHIFTS_H_
#define SHIFTS_H_

#include "oop.h"
#include <stdint.h>
#include "settings/settings.h"

typedef struct {
        uint32_t startDate;
        uint32_t startTime;
        uint32_t endDate;
        uint32_t endTime;
} ShiftData;

OOP_CLASS(Shifts) {
    OOP_METHOD(int8_t, init);
    OOP_METHOD(int8_t, keep, ShiftData *);
    OOP_METHOD(int8_t, getKeeped, ShiftData *);
    OOP_METHOD(int8_t, insert, ShiftData *);
    OOP_METHOD(int8_t, get, uint32_t, ShiftData *);
    OOP_METHOD(int8_t, getLatest, uint32_t *, ShiftData *);
    OOP_METHOD(uint32_t, getLatestIdx);
    OOP_METHOD(int8_t, reset);
};

void deleteShiftFiles();

Shifts *shifts();

#endif