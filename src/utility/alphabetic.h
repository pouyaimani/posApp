#ifndef ALPHABETIC_H_
#define ALPHABETIC_H_

#include "oop.h"
#include "stdbool.h"

OOP_CLASS(Alphebatic) {
    OOP_METHOD(Alphebatic *, enableUpperCase, bool);
    OOP_METHOD(void , addKey, bool);
    OOP_METHOD(Alphebatic *, reset);
};

Alphebatic *alphebatic();

#endif