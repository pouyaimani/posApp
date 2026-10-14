#ifndef DEV_INFO_PAGE_H
#define DEV_INFO_PAGE_H

#include "oop.h"
#include "lvgl.h"

lv_obj_t* ui_dev_info_create();

void ui_dev_info_show(lv_obj_t* page);

void ui_dev_info_hide(lv_obj_t* page);

void ui_dev_info_destroy(lv_obj_t* page);

#endif