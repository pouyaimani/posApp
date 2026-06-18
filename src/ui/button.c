#include "button.h"
#include "myColor.h"
#include "font/myFont.h"
#include "mylvgl.h"
#include "common.h"
#include "logger.h"

void ui_button_create(Button* btn, lv_obj_t* parent, unsigned int color,
                      const char* text) {
    RETURN_IF_NULL(btn, ;);
    btn->main = lv_btn_create(parent);

    LV_SET_SIZE(btn->main, 150, 30);
    LV_SET_RADIUS(btn->main, 10);
    LV_SET_BG_COLOR(btn->main, color);

    btn->textBox = lv_label_create(btn->main);
    LV_ALIGN(btn->textBox, LV_ALIGN_CENTER, 0, 0);
    LV_SET_TEXT_COLOR(btn->textBox, COLOR_WHITE);
    LV_SET_TEXT_FONT(btn->textBox, FONT_20);
    LV_SET_TEXT(btn->textBox, text);
}
