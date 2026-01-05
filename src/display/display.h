#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "oop.h"
#include "lvgl.h"

OOP_DECLARE_CLASS(Display)

OOP_CLASS(Display) {
    OOP_METHOD(void, init);
    OOP_METHOD(void, update);
    lv_obj_t *screen;
};

OOP_CTOR(Display);

Display *getDisplay(void);

#endif