#include "inbox.h"
#include "display/display.h"
#include "font/myFont.h"

#define INPUT_BOX_HEIGHT 46
#define INPUT_BOX_WIDTH 270

void ui_inBox_create(InputBox *inBox, lv_obj_t *parent) {
    inBox->main = lv_obj_create(disp()->screen);
    LV_SET_SIZE(inBox->main, INPUT_BOX_WIDTH, INPUT_BOX_HEIGHT);
    LV_SET_BORDER_COLOR(inBox->main, MAIN_THEME_COLOR);
    LV_SET_BORDER_WIDTH(inBox->main, 3);
    LV_SET_RADIUS(inBox->main, 8);
    lv_obj_set_style_shadow_opa(inBox->main, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(inBox->main, lv_color_black(), 0);
    lv_obj_set_style_shadow_offset_x(inBox->main, 0, 0);
    lv_obj_set_style_shadow_offset_y(inBox->main, 4, 0);
    lv_obj_set_style_shadow_spread(inBox->main, 0, 0);
    lv_obj_set_style_shadow_width(inBox->main, 4, 0);

    inBox->textBox = lv_label_create(inBox->main);
    LV_SET_SIZE(inBox->textBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_SET_TEXT_FONT(inBox->textBox, FONT_20);
    LV_SET_TEXT_COLOR(inBox->textBox, COLOR_BLACK);
    LV_ALIGN(inBox->textBox, LV_ALIGN_CENTER, 0, 0);
}