#ifndef ALPHABETIC_H_
#define ALPHABETIC_H_

#include "oop.h"
#include "stdbool.h"
#include "event.h"

OOP_CLASS(Alphebatic) {
    OOP_METHOD(void, addKey, char*, size_t, Key_t);
    OOP_METHOD(Alphebatic*, reset);
};

Alphebatic* alphebatic();

#endif