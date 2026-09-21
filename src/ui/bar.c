#include "bar.h"
#include "display.h"
#include "utility/utility.h"
#include "event.h"
#include "logger.h"
#include "common.h"
#include "font/myFont.h"

static void btnIncEvent(lv_event_t* e) {
    Bar* bar = (Bar*)lv_event_get_user_data(e);
    ui_bar_inc(bar);
}

static void btnDecEvent(lv_event_t* e) {
    Bar* bar = (Bar*)lv_event_get_user_data(e);
    ui_bar_dec(bar);
}

void ui_bar_set_value(Bar* bar, int value) {
    RETURN_IF_NULL(bar, ;);
    RETURN_IF_NOT(bar->max > bar->min, true, ;);

    if (value > bar->max)
        value = bar->max;

    if (value < bar->min)
        value = bar->min;

    bar->value = value;

    int pval = ((bar->value - bar->min) * 100) / (bar->max - bar->min);

    lv_bar_set_value(bar->bar, pval, LV_ANIM_ON);

    if (bar->valueLabel) {
        lv_label_set_text_fmt(bar->valueLabel, "%d", bar->value);
    }
}

void ui_bar_set_title(Bar* bar, const char* txt) {
    LV_SET_TEXT(bar->title, txt);
}

void ui_bar_inc(Bar* bar) {
    RETURN_IF_NULL(bar, ;);
    if (bar->value < bar->max) {
        bar->value++;
    }
    ui_bar_set_value(bar, bar->value);
    if (bar->cb)
        bar->cb(bar->cbData);
}

void ui_bar_dec(Bar* bar) {
    RETURN_IF_NULL(bar, ;);
    if (bar->value > bar->min) {
        bar->value--;
    }
    ui_bar_set_value(bar, bar->value);
    if (bar->cb)
        bar->cb(bar->cbData);
}

void ui_bar_show(Bar* bar) {
    RETURN_IF_NULL(bar, ;);

    LV_SHOW(bar->bar);
    LV_SHOW(bar->title);
    LV_SHOW(bar->controls);
}

void ui_bar_hide(Bar* bar) {
    RETURN_IF_NULL(bar, ;);

    LV_HIDE(bar->bar);
    LV_HIDE(bar->title);
    LV_HIDE(bar->controls);
}

void ui_bar_destroy(Bar* bar) {
    RETURN_IF_NULL(bar, ;);

    lv_obj_delete(bar->bar);
    lv_obj_delete(bar->title);
    lv_obj_delete(bar->controls);

    bar->bar        = NULL;
    bar->title      = NULL;
    bar->controls   = NULL;
    bar->btnInc     = NULL;
    bar->btnDec     = NULL;
    bar->valueLabel = NULL;
}

void ui_bar_create(Bar* bar, lv_obj_t* parent, UiBarCallback cb, void* cbData,
                   int min, int max) {
    RETURN_IF_NULL(bar, ;);
    bar->max    = max;
    bar->min    = min;
    bar->value  = min;
    bar->cb     = cb;
    bar->cbData = cbData;

    static lv_style_t style_bg;
    static lv_style_t style_indic;

    lv_style_init(&style_bg);
    lv_style_set_border_color(&style_bg, lv_color_hex(MAIN_THEME_COLOR));
    lv_style_set_border_width(&style_bg, 2);
    lv_style_set_pad_all(&style_bg, 6); /*To make the indicator smaller*/
    lv_style_set_radius(&style_bg, 6);
    lv_style_set_anim_duration(&style_bg, 1000);

    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic, lv_color_hex(MAIN_THEME_COLOR));
    lv_style_set_radius(&style_indic, 3);

    bar->bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar->bar); /*To have a clean start*/
    lv_obj_add_style(bar->bar, &style_bg, 0);
    lv_obj_add_style(bar->bar, &style_indic, LV_PART_INDICATOR);

    lv_obj_set_size(bar->bar, lv_pct(80), 40);
    lv_obj_center(bar->bar);

    bar->title = lv_label_create(parent);
    LV_SET_TEXT_FONT(bar->title, FONT_20);
    LV_SET_TEXT_COLOR(bar->title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(bar->title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(bar->title, lv_pct(80), LV_SIZE_CONTENT);
    LV_ALIGN(bar->title, LV_ALIGN_TOP_MID, 0, 20);

    ui_bar_set_value(bar, bar->min);

    lv_obj_t* controls = lv_obj_create(parent);

    bar->controls = controls;

    lv_obj_remove_style_all(controls);

    lv_obj_set_size(controls, lv_pct(80), 50);
    lv_obj_align_to(controls, bar->bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    bar->btnDec = lv_button_create(controls);
    lv_obj_set_size(bar->btnDec, 55, 42);
    lv_obj_set_style_radius(bar->btnDec, 8, 0);
    lv_obj_set_style_bg_color(bar->btnDec, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_color(bar->btnDec, lv_color_hex(MAIN_THEME_COLOR),
                                  0);
    lv_obj_set_style_border_width(bar->btnDec, 2, 0);

    lv_obj_add_event_cb(bar->btnDec, btnDecEvent, LV_EVENT_CLICKED, bar);

    lv_obj_t* lblDec = lv_label_create(bar->btnDec);
    lv_label_set_text(lblDec, "-");
    LV_SET_TEXT_FONT(lblDec, FONT_20);
    LV_SET_TEXT_COLOR(lblDec, MAIN_THEME_COLOR);
    lv_obj_center(lblDec);

    bar->valueLabel = lv_label_create(controls);

    lv_label_set_text_fmt(bar->valueLabel, "%d", bar->value);

    LV_SET_TEXT_FONT(bar->valueLabel, FONT_20);
    LV_SET_TEXT_COLOR(bar->valueLabel, COLOR_BLACK);

    lv_obj_set_width(bar->valueLabel, 70);
    lv_obj_set_style_text_align(bar->valueLabel, LV_TEXT_ALIGN_CENTER, 0);

    bar->btnInc = lv_button_create(controls);
    lv_obj_set_size(bar->btnInc, 55, 42);
    lv_obj_set_style_radius(bar->btnInc, 8, 0);
    lv_obj_set_style_bg_color(bar->btnInc, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_color(bar->btnInc, lv_color_hex(MAIN_THEME_COLOR),
                                  0);
    lv_obj_set_style_border_width(bar->btnInc, 2, 0);

    lv_obj_add_event_cb(bar->btnInc, btnIncEvent, LV_EVENT_CLICKED, bar);

    lv_obj_t* lblInc = lv_label_create(bar->btnInc);
    lv_label_set_text(lblInc, "+");
    LV_SET_TEXT_COLOR(lblInc, MAIN_THEME_COLOR);
    LV_SET_TEXT_FONT(lblInc, FONT_20);
    lv_obj_center(lblInc);
}