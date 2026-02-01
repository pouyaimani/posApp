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

lv_obj_t *uiMenu(lv_obj_t * parent) {
    lv_obj_t * menu = lv_obj_create(parent);

    /* Size & positioning */
    lv_obj_set_width(menu, lv_pct(100));
    lv_obj_set_height(menu, lv_pct(100));
    lv_obj_center(menu);

    /* Vertical layout */
    lv_obj_set_layout(menu, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu,
                           LV_FLEX_ALIGN_START,   /* main axis */
                           LV_FLEX_ALIGN_START,   /* cross axis */
                           LV_FLEX_ALIGN_START);  /* track align */

    /* Optional spacing between items */
    lv_obj_set_style_pad_row(menu, 8, 0);
    lv_obj_set_style_pad_column(menu, 0, 0);

    return menu;
}

lv_obj_t *uiMenuAddItem(lv_obj_t * menu,
                         const char * text,
                         lv_event_cb_t event_cb,
                         void * user_data) {
    lv_obj_t * btn = lv_btn_create(menu);

    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, LV_SIZE_CONTENT);

    if(event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, user_data);
    }

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}
