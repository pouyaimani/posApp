#ifndef BAR_H_
#define BAR_H_

#include "oop.h"
#include "lvgl.h"

/***************************** Bar *****************************/

OOP_CLASS(Bar) {
    lv_obj_t* bar;
    lv_obj_t* title;
    int       value;
    int       max;
    int       min;
};

void ui_bar_destroy(Bar* bar);

void ui_bar_create(Bar* bar, lv_obj_t* parent, int min, int max);

void ui_bar_set_value(Bar* bar, int value);

void ui_bar_set_title(Bar* bar, const char* txt);

void ui_bar_inc(Bar* bar);

void ui_bar_dec(Bar* bar);

void ui_bar_show(Bar* bar);

void ui_bar_hide(Bar* bar);

#endif