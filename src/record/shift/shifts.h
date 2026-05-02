#ifndef SHIFTS_H_
#define SHIFTS_H_

#include "oop.h"
#include <stdint.h>
#include "settings/settings.h"

OOP_CLASS(Shiftss) {
    OOP_METHOD(void, init);
    OOP_METHOD(int, insert, uint32_t index, ShiftData *);
    OOP_METHOD(int, flush);
    OOP_METHOD(int, get, uint32_t, ShiftData *);
    OOP_METHOD(void, reset);
};

Shiftss *shifts();

#endif