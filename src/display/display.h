#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "oop.h"
#include "lvgl.h"

OOP_DECLARE_CLASS(Display)

OOP_CLASS(Display) {
    OOP_METHOD(void, init);
    OOP_METHOD(void, update);
    const char *name;
};

OOP_CTOR(Display, const char* name);

Display *getDisplay(void);

#endif