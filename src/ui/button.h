#ifndef BUTTON_H_
#define BUTTON_H_

#include "oop.h"
#include "lvgl.h"

OOP_CLASS(Button) {
    lv_obj_t* main;
    lv_obj_t* textBox;
};

void ui_button_create(Button* btn, lv_obj_t* parent, unsigned int color,
                      const char* text);

#endif