#include "ui.h"
#include "display.h"

#define INPUT_BOX_HEIGHT 46
#define INPUT_BOX_WIDTH 270

InputBox uiInputBox(lv_obj_t *parent) {
    InputBox inBox;
    inBox.main = lv_obj_create(getDisplay()->screen);
    lv_obj_set_size(inBox.main, INPUT_BOX_WIDTH, INPUT_BOX_HEIGHT);
    lv_obj_set_style_border_color(inBox.main, lv_color_hex(MAIN_THEME_COLOR), 0);
    lv_obj_set_style_border_width(inBox.main, 3, 0);
    lv_obj_set_style_shadow_opa(inBox.main, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(inBox.main, lv_color_black(), 0);
    lv_obj_set_style_shadow_offset_x(inBox.main, 0, 0);
    lv_obj_set_style_shadow_offset_y(inBox.main, 4, 0);
    lv_obj_set_style_shadow_spread(inBox.main, 0, 0);
    lv_obj_set_style_shadow_width(inBox.main, 4, 0);

    inBox.textBox = lv_label_create(inBox.main);
    lv_obj_set_size(inBox.textBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(inBox.textBox, lv_color_black(), 0);
    lv_obj_align(inBox.textBox, LV_ALIGN_CENTER, 0, 0);
    return inBox;
}

lv_obj_t *uiConfirmButton(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, 150, 50);
    lv_obj_set_style_radius(obj, 40, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x68DD40), 0);
    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x38BA7C), 0);
    // lv_obj_t *child = lv_obj_create(obj);
    // lv_obj_set_size(child, 122, 40);
    // lv_obj_set_style_radius(child, 40, 0);
    // lv_obj_set_style_bg_color(child, lv_color_hex(0x68DD40), 0);
    // lv_obj_set_style_bg_grad_color(child, lv_color_hex(0x38BA7C), 0);
    // lv_obj_align(child, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    // lv_obj_set_style_bg_opa(child, LV_OPA_20, 0);
    return obj;
}

lv_obj_t *uiCancellButton(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, 150, 50);
    lv_obj_set_style_radius(obj, 40, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFF4E4E), 0);
    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0xFF2A6A), 0);
    return obj;
}