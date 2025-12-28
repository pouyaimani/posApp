#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "oop.h"
#include "lvgl.h"

OOP_DECLARE_CLASS(Display)

OOP_CLASS(Display) {
    OOP_IMETHOD(void, Display, init);
    OOP_IMETHOD(void, Display, update);
    const char *name;
};

OOP_CTOR(Display, const char* name);

Display *getDisplay(void);

#endif