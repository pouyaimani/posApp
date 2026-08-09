#include "button.h"
#include <string.h>
#include "font/myFont.h"
#include "myColor.h"

#define BTN_W 150
#define BTN_H 35

#define ICON_SIZE     26
#define ACCENT_WIDTH  135
#define BUTTON_RADIUS 8

#define BG_COLOR   0xFFFFFF
#define BLUE_COLOR MAIN_THEME_COLOR
#define TEXT_COLOR 0xFFFFFF

void ui_button_create(Button* btn, lv_obj_t* parent) {
    memset(btn, 0, sizeof(*btn));

    btn->root = lv_obj_create(parent);

    lv_obj_remove_style_all(btn->root);

    lv_obj_set_size(btn->root, BTN_W, BTN_H);

    lv_obj_clear_flag(btn->root, LV_OBJ_FLAG_SCROLLABLE);

    btn->bg = lv_obj_create(btn->root);

    lv_obj_remove_style_all(btn->bg);

    lv_obj_set_size(btn->bg, BTN_W, BTN_H);

    lv_obj_center(btn->bg);

    lv_obj_set_style_radius(btn->bg, BUTTON_RADIUS, 0);

    // lv_obj_set_style_border_width(btn->bg, 1, 0);

    // lv_obj_set_style_border_opa(btn->bg, LV_OPA_100, 0);

    lv_obj_set_style_bg_opa(btn->bg, LV_OPA_100, 0);

    /*
     * Blue right panel
     */

    // btn->accent = lv_obj_create(btn->bg);

    // lv_obj_remove_style_all(btn->accent);

    // lv_obj_set_size(btn->accent, ACCENT_WIDTH, BTN_H - 4);

    // lv_obj_align(btn->accent, LV_ALIGN_CENTER, 0, 0);

    // lv_obj_set_style_bg_opa(btn->accent, LV_OPA_100, 0);

    // lv_obj_set_style_radius(btn->accent, BUTTON_RADIUS, 0);

    // lv_obj_set_style_bg_color(btn->accent, lv_color_hex(BG_COLOR), 0);

    /*
     * Icon
     */

    btn->icon = lv_label_create(btn->bg);

    lv_label_set_text(btn->icon, LV_SYMBOL_GPS);

    lv_obj_set_style_text_color(btn->icon, lv_color_hex(TEXT_COLOR), 0);

    lv_obj_set_style_text_font(btn->icon, LV_FONT_DEFAULT, 0);

    lv_obj_align(btn->icon, LV_ALIGN_LEFT_MID, 18, 0);

    /*
     * Text
     */

    btn->label = lv_label_create(btn->bg);

    lv_label_set_text(btn->label, "");
    lv_obj_set_style_text_font(btn->label, &FONT_16, 0);

    lv_obj_set_style_text_color(btn->label, lv_color_hex(TEXT_COLOR), 0);

    lv_obj_align_to(btn->label, btn->icon, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
}

void ui_button_set_text(Button* btn, const char* text) {
    lv_label_set_text(btn->label, text);
}

void ui_button_set_icon(Button* btn, const char* symbol) {
    lv_label_set_text(btn->icon, symbol);
}

void ui_button_set_size(Button* btn, int w, int h) {
    lv_obj_set_size(btn->root, w, h);

    lv_obj_set_size(btn->bg, w, h);

    // lv_obj_set_size(btn->accent, w / 3, h);
}

void ui_button_set_color(Button* btn, lv_color_t color) {

    // lv_obj_set_style_text_color(btn->label, color, 0);

    // lv_obj_set_style_text_color(btn->icon, color, 0);

    lv_obj_set_style_bg_color(btn->bg, color, 0);

    lv_obj_set_style_border_color(btn->bg, color, 0);
}

lv_obj_t* ui_button_obj(Button* btn) { return btn->root; }