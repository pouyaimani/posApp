#ifndef LOCATION_BUTTON_H
#define LOCATION_BUTTON_H

#include "lvgl.h"

typedef struct {
    lv_obj_t* root;

    lv_obj_t* bg;
    lv_obj_t* accent;

    lv_obj_t* icon;
    lv_obj_t* label;

} Button;

void ui_button_create(Button* btn, lv_obj_t* parent);

void ui_button_set_text(Button* btn, const char* text);

void ui_button_set_icon(Button* btn, const char* symbol);

void ui_button_set_size(Button* btn, int w, int h);

void ui_button_set_color(Button* btn, lv_color_t color);

lv_obj_t* ui_button_obj(Button* btn);

#endif