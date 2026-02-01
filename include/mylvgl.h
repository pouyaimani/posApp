#ifndef MY_LVGL_H
#define MY_LVGL_H

#include "lvgl.h"

#define LV_HIDE(obj)                                   lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)
#define LV_SHOW(obj)                                   lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN)

#define LV_SET_SIZE(obj, x, y)                         lv_obj_set_size(obj, x, y)

#define LV_SET_BG_COLOR(obj, color)                    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN)
#define LV_SET_BORDER_COLOR(obj, color)                lv_obj_set_style_border_color(obj, color, LV_PART_MAIN)

#define LV_ALIGN(obj, align, x, y)                     lv_obj_align(obj, align, x, y)

#define LV_SET_RADIUS(obj, radius)                     lv_obj_set_style_radius(obj, radius, LV_PART_MAIN)

#define LV_SET_TEXT(obj, text)                         lv_label_set_text(obj, text)
#endif