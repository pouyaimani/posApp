#ifndef MY_LVGL_H
#define MY_LVGL_H

#include "lvgl.h"

/***********************************Style & Appearance***************************************/

#define LV_SET_BG_COLOR(obj, color) \
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN)

#define LV_SET_BG_OPA(obj, opa) \
    lv_obj_set_style_bg_opa(obj, opa, LV_PART_MAIN)

#define LV_SET_BORDER_COLOR(obj, color) \
    lv_obj_set_style_border_color(obj, lv_color_hex(color), LV_PART_MAIN)

#define LV_SET_BORDER_OPA(obj, opa) \
    lv_obj_set_style_border_opa(obj, opa, LV_PART_MAIN)

#define LV_SET_BORDER_WIDTH(obj, width) \
    lv_obj_set_style_border_width(obj, width, LV_PART_MAIN)

#define LV_SET_BORDER_SIDE(obj, side) \
    lv_obj_set_style_border_side(obj, side, LV_PART_MAIN)

#define LV_HIDE_BORDER(obj) \
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_NONE, LV_PART_MAIN)

#define LV_HIDE_BORDER_SIDE(obj, border) \
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL & ~border, LV_PART_MAIN);

#define LV_SET_RADIUS(obj, value) \
    lv_obj_set_style_radius(obj, value, LV_PART_MAIN)

#define LV_SET_OUTLINE_WIDTH(obj, width) \
    lv_obj_set_style_outline_width(obj, width, LV_PART_MAIN)

#define LV_SET_OUTLINE_COLOR(obj, color) \
    lv_obj_set_style_outline_color(obj, lv_color_hex(color), LV_PART_MAIN)

#define LV_CLIP_CORNER(obj) \
    lv_obj_set_style_clip_corner(obj, LV_STYLE_CLIP_CORNER, LV_PART_MAIN)

/***********************************************************************************************/


/***************************************Size & Position*****************************************/

#define LV_SET_SIZE(obj, w, h) \
    lv_obj_set_size(obj, w, h)

#define LV_SET_WIDTH(obj, w) \
    lv_obj_set_width(obj, w)

#define LV_GET_WIDTH(obj) \
    lv_obj_get_width(obj)

#define LV_SET_HEIGHT(obj, h) \
    lv_obj_set_height(obj, h)

#define LV_GET_HEIGHT(obj) \
    lv_obj_get_height(obj)

#define LV_AUTO_WIDTH(obj) \
    lv_obj_set_width(obj, LV_SIZE_CONTENT)

#define LV_AUTO_HEIGHT(obj) \
    lv_obj_set_height(obj, LV_SIZE_CONTENT)

#define LV_AUTO_SIZE(obj) \
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT)

#define LV_SET_X(obj, x) \
    lv_obj_set_x(obj, x)

#define LV_SET_Y(obj, y) \
    lv_obj_set_y(obj, y)

#define LV_GET_WIDTH(obj) \
    lv_obj_get_width(obj)

#define LV_GET_HEIGHT(obj) \
    lv_obj_get_height(obj)


/***********************************************************************************************/

/********************************************Padding********************************************/

#define LV_SET_PAD_ALL(obj, pad) \
    lv_obj_set_style_pad_all(obj, pad, LV_PART_MAIN)

#define LV_SET_PAD_TOP(obj, pad) \
    lv_obj_set_style_pad_top(obj, pad, LV_PART_MAIN)

#define LV_SET_PAD_BOTTOM(obj, pad) \
    lv_obj_set_style_pad_bottom(obj, pad, LV_PART_MAIN)

#define LV_SET_PAD_LEFT(obj, pad) \
    lv_obj_set_style_pad_left(obj, pad, LV_PART_MAIN)

#define LV_SET_PAD_RIGHT(obj, pad) \
    lv_obj_set_style_pad_right(obj, pad, LV_PART_MAIN)

#define LV_SET_ROW_PAD(obj, pad) \
    lv_obj_set_style_pad_row(obj, pad, 0)

#define LV_SET_COLUMN_PAD(obj, pad) \
    lv_obj_set_style_pad_column(obj, pad, 0)


/***********************************************************************************************/

/**************************************Alignment & Layout***************************************/

#define LV_SET_ALIGN(obj, align) \
    lv_obj_set_align(obj, (lv_align_t)(align))

#define LV_ALIGN(obj, align, x, y) \
    lv_obj_align(obj, (lv_align_t)(align), x, y)

#define LV_ALIGN_TO(obj, base, align, x, y) \
    lv_obj_align_to(obj, base, (lv_align_t)(align), x, y)

#define LV_SET_FLEX_FLOW(obj, flow) \
    lv_obj_set_flex_flow(obj, (lv_flex_flow_t)(flow))

#define LV_SET_FLEX_ALIGN(obj, main, cross, track) \
    lv_obj_set_flex_align(obj, \
        (lv_flex_align_t)(main), \
        (lv_flex_align_t)(cross), \
        (lv_flex_align_t)(track))


/***********************************************************************************************/

/************************************Scrolling & Visibility*************************************/

#define LV_SCROLL_ENABLE(obj) \
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_ACTIVE)

#define LV_SCROLL_DISABLE(obj) \
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF)

#define LV_SCROLL_TO_VIEW(obj) \
    lv_obj_scroll_to_view(obj, LV_ANIM_ON)

#define LV_HIDE(obj) \
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)

#define LV_SHOW(obj) \
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN)


/***********************************************************************************************/

/*****************************************State & Control***************************************/

#define LV_ENABLE(obj) \
    lv_obj_clear_state(obj, LV_STATE_DISABLED)

#define LV_DISABLE(obj) \
    lv_obj_add_state(obj, LV_STATE_DISABLED)

#define LV_CLEAN(obj) \
    lv_obj_clean(obj)

/***********************************************************************************************/

/**********************************************Text*********************************************/

#define LV_SET_TEXT(obj, text) \
    lv_label_set_text(obj, text)

#define LV_SET_TEXT_ALIGN(obj, align) \
    lv_obj_set_style_text_align(obj, align, LV_PART_MAIN)

#define LV_SET_TEXT_COLOR(obj, color) \
    lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN)

/***********************************************************************************************/

#define LV_PARENT(obj) \
    lv_obj_get_parent(obj)

#endif