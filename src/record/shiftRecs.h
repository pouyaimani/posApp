#ifndef SHIFTS_H_
#define SHIFTS_H_

#include "oop.h"
#include <stdint.h>
#include "settings/settings.h"

typedef struct __attribute__((packed)) {
        uint32_t startDate;
        uint32_t startTime;
        uint32_t endDate;
        uint32_t endTime;
} ShiftData;

OOP_CLASS(Shifts) {
    OOP_METHOD(int, keep, ShiftData *);
    OOP_METHOD(int, getKeeped, ShiftData *);
    OOP_METHOD(int, insert, ShiftData *);
    OOP_METHOD(int, get, uint32_t, ShiftData *);
    OOP_METHOD(int, getLatest, uint32_t *, ShiftData *);
    OOP_METHOD(uint32_t, getLatestIdx);
    OOP_METHOD(void, reset);
};

Shifts *shifts();

#endif