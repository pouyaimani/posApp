#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "oop.h"
#include "myLvgl.h"
#include "myColor.h"

OOP_DECLARE_CLASS(Display)

OOP_CLASS(Display) {
    OOP_METHOD(void, init);
    OOP_METHOD(void, update);
    lv_obj_t *fscreen;
    lv_obj_t *screen;
    lv_obj_t *statusbar;
};

OOP_CTOR(Display);

Display *disp(void);

#endif