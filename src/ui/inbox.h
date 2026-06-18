#ifndef IN_BOX_H_
#define IN_BOX_H_

#include "oop.h"
#include "lvgl.h"

OOP_CLASS(InputBox) {
    lv_obj_t* main;
    lv_obj_t* textBox;
    lv_obj_t* cursorLine;
};

void ui_inBox_create(InputBox* inBox, lv_obj_t* parent);

#endif