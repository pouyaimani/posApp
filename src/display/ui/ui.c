#include "ui.h"
#include "display.h"
#include "utility/utility.h"
#include "event.h"

#define INPUT_BOX_HEIGHT 46
#define INPUT_BOX_WIDTH 270

InputBox uiInputBox(lv_obj_t *parent) {
    InputBox inBox;
    inBox.main = lv_obj_create(getDisplay()->screen);
    LV_SET_SIZE(inBox.main, INPUT_BOX_WIDTH, INPUT_BOX_HEIGHT);
    LV_SET_BORDER_COLOR(inBox.main, MAIN_THEME_COLOR);
    LV_SET_BORDER_WIDTH(inBox.main, 3);
    lv_obj_set_style_shadow_opa(inBox.main, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(inBox.main, lv_color_black(), 0);
    lv_obj_set_style_shadow_offset_x(inBox.main, 0, 0);
    lv_obj_set_style_shadow_offset_y(inBox.main, 4, 0);
    lv_obj_set_style_shadow_spread(inBox.main, 0, 0);
    lv_obj_set_style_shadow_width(inBox.main, 4, 0);

    inBox.textBox = lv_label_create(inBox.main);
    LV_SET_SIZE(inBox.textBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_SET_TEXT_FONT(inBox.textBox, FONT_20);
    LV_SET_TEXT_COLOR(inBox.textBox, COLOR_BLACK);
    LV_ALIGN(inBox.textBox, LV_ALIGN_CENTER, 0, 0);
    return inBox;
}

Button uiButton(lv_obj_t *parent, unsigned int color, const char * text) {
    Button btn;
    btn.main = lv_btn_create(parent);

    LV_SET_SIZE(btn.main, 150, 30);
    LV_SET_RADIUS(btn.main, 10);
    LV_SET_BG_COLOR(btn.main, color);

    btn.textBox = lv_label_create(btn.main);
    LV_ALIGN(btn.textBox, LV_ALIGN_CENTER, 0, 0);
    LV_SET_TEXT_COLOR(btn.textBox, COLOR_WHITE);
    LV_SET_TEXT_FONT(btn.textBox, FONT_20);
    LV_SET_TEXT(btn.textBox, text);
    return btn;
}

static void addItem(Menu *menu, const char * text,
                lv_event_cb_t event_cb, void * user_data) {
    lv_obj_t *btn = lv_btn_create(menu->main);
    
    LV_SET_SIZE(btn, lv_pct(100), LV_SIZE_CONTENT);
    LV_SET_BG_OPA(btn, LV_OPA_0);
    LV_SET_BORDER_OPA(btn, LV_OPA_0);

    if(event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, user_data);
    }

    lv_obj_t * label = lv_label_create(btn);
    LV_SET_SIZE(label, lv_pct(100), LV_SIZE_CONTENT);
    LV_SET_BG_OPA(label, LV_OPA_0);
    LV_SET_BORDER_OPA(label, LV_OPA_0);
    LV_SET_TEXT_FONT(label, FONT_20);
    LV_ALIGN(label, LV_ALIGN_CENTER, 0, 0);
    LV_SET_TEXT_ALIGN(label, LV_TEXT_ALIGN_RIGHT);
    char str[64];
    char num[4];
    memset(str, 0, sizeof(str));
    memset(num, 0, sizeof(num));
    snprintf(num, sizeof(num), "%d", menu->cnt + 1);
    snprintf(str, sizeof(str), "%s.%s", num, text);
    lv_obj_set_style_base_dir(label, LV_BASE_DIR_RTL, 0);
    LV_SET_TEXT(label, str);
    
    menu->item[menu->cnt++] = btn;
}

static void handleItem(Menu *menu, Key_t key) {
    MenuUpDown_t updown;
    if (key == KEY_UP) {
        updown = MENU_UP;
    } else if (key == KEY_DOWN) {
        updown = MENU_DOWN;
    } else {
        return;
    }
    LV_SET_TEXT_COLOR(menu->item[menu->idx], COLOR_BLACK);
    if (updown == MENU_UP) {
        menu->idx = menu->idx == 0 ? menu->cnt - 1 : menu->idx - 1;
    } else if (updown == MENU_DOWN) {
        menu->idx = menu->idx == (menu->cnt - 1) ? 0 : menu->idx + 1;
    }
    LV_SET_TEXT_COLOR(menu->item[menu->idx], 0x68DD40);
}

static void menuShow(Menu *menu) {
    if (menu->cnt > 0) {
        LV_SET_TEXT_COLOR(menu->item[0], 0x68DD40);
        for(int i = 1 ; i < menu->cnt ; i++) {
            LV_SET_TEXT_COLOR(menu->item[i], COLOR_BLACK);
        }
    }
    menu->idx = 0;
    LV_SHOW(menu->main);
}

static void menuHide(Menu *menu) {
    LV_HIDE(menu->main);
}

static void menuGetIdx(Menu *menu) {
    return menu->idx;
}

Menu uiMenu(lv_obj_t * parent) {
    Menu menu;
    menu.vtable.addItem = addItem;
    menu.vtable.handleItem = handleItem;
    menu.vtable.show = menuShow;
    menu.vtable.hide = menuHide;
    menu.vtable.getIdx = menuGetIdx;
    menu.cnt = 0;
    menu.idx = 0;

    menu.main = lv_obj_create(parent);

    /* Size & positioning */
    LV_SET_SIZE(menu.main, lv_pct(100), lv_pct(100));
    LV_ALIGN(menu.main, LV_ALIGN_CENTER, 0, 0);

    /* Vertical layout */
    LV_SET_FLEX_FLOW(menu.main, LV_FLEX_FLOW_COLUMN);
    LV_SET_FLEX_ALIGN(menu.main,
                            LV_FLEX_ALIGN_START,   /* main axis */
                           LV_FLEX_ALIGN_START,   /* cross axis */
                           LV_FLEX_ALIGN_START);  /* track align */

    /* Optional spacing between items */
    LV_SET_ROW_PAD(menu.main, 8);
    return menu;
}