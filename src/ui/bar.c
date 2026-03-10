#include "ui.h"
#include "display.h"
#include "utility/utility.h"
#include "event.h"

static void barSetValue(Bar *bar, int value) {
    bar->value = value > bar->max ? bar->max : value;
    int pval = (bar->value * 100) / bar->max;
    lv_bar_set_value(bar->bar, pval, LV_ANIM_ON);
}

static void barSetTitle(Bar *bar, const char *txt) {
    LV_SET_TEXT(bar->title, txt);
}

static void barInc(Bar *bar) {
    if (bar->value < bar->max) {
        bar->value++;
    }
    barSetValue(bar, bar->value);
}

static void barDec(Bar *bar) {
    if (bar->value > bar->min) {
        bar->value--;
    }
    barSetValue(bar, bar->value);
}

static void barShow(Bar *bar) {
    LV_SHOW(bar->bar);
    LV_SHOW(bar->title);
}

static void barHide(Bar *bar) {
    LV_HIDE(bar->bar);
    LV_HIDE(bar->title);
}

void uiBarDelete(Bar *bar) {
    lv_obj_delete(bar->bar);
    lv_obj_delete(bar->title);
}

void uiBar(Bar *bar, lv_obj_t * parent, int min, int max)
{
    bar->vtable.decrease = barDec;
    bar->vtable.increase = barInc;
    bar->vtable.setValue = barSetValue;
    bar->vtable.show = barShow;
    bar->vtable.hide = barHide;
    bar->vtable.setTitle= barSetTitle;
    bar->max = max;
    bar->min = min;
    bar->value = min;

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
    lv_obj_remove_style_all(bar->bar);  /*To have a clean start*/
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

    barSetValue(bar, bar->min);
}