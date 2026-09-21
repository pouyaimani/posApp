#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include "lvgl.h"

typedef struct {
    lv_obj_t* bar;
    lv_obj_t* title;
    lv_obj_t* valueLabel;

    int value;
} ProgressBar;

void ui_progress_bar_create(ProgressBar* progress, lv_obj_t* parent);

void ui_progress_bar_set_value(ProgressBar* progress, int value);
void ui_progress_bar_set_title(ProgressBar* progress, const char* title);

void ui_progress_bar_show(ProgressBar* progress);
void ui_progress_bar_hide(ProgressBar* progress);
void ui_progress_bar_destroy(ProgressBar* progress);

#endif