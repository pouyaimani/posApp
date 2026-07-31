#include "menu.h"
#include "display.h"
#include "utility/utility.h"
#include "event.h"
#include "state.h"
#include "logger.h"
#include "phrases/phrases.h"
#include "common.h"

void ui_menu_addItem(Menu* menu, const char* text, lv_text_align_t txtAlign,
                     State* state, CallBack_t cb, void* user_data) {
    RETURN_IF_NULL(menu, ;);
    if (menu->cnt >= MENU_ITEM_MAX) {
        LOG_ERROR("Menu full");
        return;
    }
    lv_obj_t* btn = lv_btn_create(menu->main);

    LV_SET_SIZE(btn, lv_pct(100), LV_SIZE_CONTENT);
    LV_SET_BG_OPA(btn, LV_OPA_0);
    LV_SET_BORDER_OPA(btn, LV_OPA_0);
    LV_ALIGN(btn, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* number = lv_label_create(btn);
    LV_SET_SIZE(number, lv_pct(10), LV_SIZE_CONTENT);
    LV_SET_BG_OPA(number, LV_OPA_0);
    LV_SET_BORDER_OPA(number, LV_OPA_0);
    LV_SET_TEXT_FONT(number, FONT_20);
    LV_ALIGN(number,
             txtAlign == LV_TEXT_ALIGN_LEFT ? LV_ALIGN_LEFT_MID
                                            : LV_ALIGN_RIGHT_MID,
             0, 0);
    LV_SET_TEXT_ALIGN(number, txtAlign == LV_TEXT_ALIGN_LEFT
                                  ? LV_TEXT_ALIGN_RIGHT
                                  : LV_TEXT_ALIGN_LEFT);

    lv_obj_t* label = lv_label_create(btn);
    LV_SET_SIZE(label, lv_pct(90), LV_SIZE_CONTENT);
    LV_SET_BG_OPA(label, LV_OPA_0);
    LV_SET_BORDER_OPA(label, LV_OPA_0);
    LV_SET_TEXT_FONT(label, FONT_20);
    LV_ALIGN(label,
             txtAlign == LV_TEXT_ALIGN_LEFT ? LV_ALIGN_RIGHT_MID
                                            : LV_ALIGN_LEFT_MID,
             txtAlign == LV_TEXT_ALIGN_LEFT ? 5 : -5, 0);
    LV_SET_TEXT_ALIGN(label, txtAlign);
    lv_obj_set_style_base_dir(label, LV_BASE_DIR_RTL, 0);
    lv_obj_set_style_base_dir(number, LV_BASE_DIR_AUTO, 0);
    lv_label_set_text_fmt(
        number, txtAlign == LV_TEXT_ALIGN_LEFT ? "%d." : ".%d", menu->cnt + 1);
    LV_SET_TEXT(label, text);

    menu->item[menu->cnt]     = btn;
    menu->state[menu->cnt]    = state;
    menu->cb[menu->cnt]       = cb;
    menu->userData[menu->cnt] = user_data;
    menu->cnt++;
}

void ui_menu_add_on_off_item(Menu* menu, const char* text, bool toggle,
                             State* state, CallBack_t cb, void* user_data) {
    RETURN_IF_NULL(menu, ;);
    if (menu->cnt >= MENU_ITEM_MAX) {
        LOG_ERROR("Menu full");
        return;
    }
    lv_obj_t* btn = lv_btn_create(menu->main);

    LV_SET_SIZE(btn, lv_pct(100), LV_SIZE_CONTENT);
    LV_SET_BG_OPA(btn, LV_OPA_0);
    LV_SET_BORDER_OPA(btn, LV_OPA_0);
    LV_ALIGN(btn, LV_ALIGN_CENTER, 0, 0);

    /* left value */
    lv_obj_t* value = lv_label_create(btn);
    LV_SET_SIZE(value, lv_pct(20), LV_SIZE_CONTENT);
    LV_SET_TEXT(value, toggle ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    LV_SET_TEXT_COLOR(value, toggle ? 0x00ff00 : 0xcc0000);
    LV_SET_TEXT_ALIGN(value, LV_TEXT_ALIGN_LEFT);
    LV_ALIGN(value, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* label = lv_label_create(btn);
    LV_SET_SIZE(label, lv_pct(80), LV_SIZE_CONTENT);
    LV_SET_BG_OPA(label, LV_OPA_0);
    LV_SET_BORDER_OPA(label, LV_OPA_0);
    LV_SET_TEXT_FONT(label, FONT_20);
    LV_SET_TEXT_ALIGN(label, LV_TEXT_ALIGN_RIGHT);
    LV_ALIGN(label, LV_ALIGN_RIGHT_MID, 0, 0);
    char str[64];
    char num[4];
    memset(str, 0, sizeof(str));
    memset(num, 0, sizeof(num));
    snprintf(num, sizeof(num), "%d", menu->cnt + 1);
    snprintf(str, sizeof(str), "%s.%s", num, text);
    lv_obj_set_style_base_dir(label, LV_BASE_DIR_RTL, 0);
    LV_SET_TEXT(label, str);

    menu->item[menu->cnt]   = btn;
    menu->state[menu->cnt]  = state;
    menu->cb[menu->cnt]     = cb;
    menu->toggle[menu->cnt] = toggle;
    menu->cnt++;
}

static void showSelector(Menu* menu) {
    RETURN_IF_NULL(menu, ;);
    LV_SHOW(menu->selector);
    lv_obj_update_layout(menu->main);
    lv_obj_set_size(menu->selector, lv_pct(105),
                    LV_GET_HEIGHT(menu->item[menu->idx]) * 1.2);
    lv_obj_align_to(menu->selector, menu->item[menu->idx], LV_ALIGN_CENTER, 0,
                    0);
}

void ui_menu_handleItem(Menu* menu, Key_t key) {
    RETURN_IF_NULL(menu, ;);
    MenuUpDown_t updown;
    if (key == KEY_UP) {
        updown = MENU_UP;
    } else if (key == KEY_DOWN) {
        updown = MENU_DOWN;
    } else {
        return;
    }
    if (updown == MENU_UP) {
        menu->idx = menu->idx == 0 ? menu->cnt - 1 : menu->idx - 1;
    } else if (updown == MENU_DOWN) {
        menu->idx = menu->idx == (menu->cnt - 1) ? 0 : menu->idx + 1;
    }
    showSelector(menu);
    lv_obj_scroll_to_view(menu->item[menu->idx], LV_ANIM_ON);
}

void ui_menu_show(Menu* menu) {
    RETURN_IF_NULL(menu, ;);
    menu->idx = 0;
    LV_SHOW(menu->main);
    if (menu->cnt > 0) {
        showSelector(menu);
    }
}

void ui_menu_hide(Menu* menu) {
    RETURN_IF_NULL(menu, ;);
    LV_HIDE(menu->main);
}

int ui_menu_get_idx(Menu* menu) {
    RETURN_IF_NULL(menu, ;);
    return menu->idx;
}

void ui_menu_set_checked(Menu* menu, int newIdx) {
    RETURN_IF_NULL(menu, ;);
    if (newIdx < 0 || newIdx >= menu->cnt)
        return;
    lv_obj_update_layout(menu->main);
    LV_SET_SIZE(menu->checker, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_align_to(menu->checker, menu->item[newIdx], LV_ALIGN_CENTER, 0, 0);
    LV_SET_TEXT_ALIGN(menu->checker, LV_TEXT_ALIGN_LEFT);
    LV_SHOW(menu->checker);
}

void ui_menu_toggle(Menu* menu, int idx) {
    RETURN_IF_NULL(menu, ;);
    lv_obj_t* obj     = lv_obj_get_child(menu->item[idx], 0);
    menu->toggle[idx] = !menu->toggle[idx];
    LV_SET_TEXT(obj, menu->toggle[idx] ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    LV_SET_TEXT_COLOR(obj, menu->toggle[idx] ? 0x00ff00 : 0xcc0000);
}

int8_t ui_menu_create(Menu* menu, lv_obj_t* parent) {
    RETURN_VALUE_IF_NULL(menu, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(parent, ;, ERR_NULL_PARAMETER);
    menu->cnt         = 0;
    menu->idx         = 0;
    menu->selected    = 0;
    menu->checkEnable = false;
    for (size_t i = 0; i < MENU_ITEM_MAX; i++) {
        menu->item[i]   = NULL;
        menu->cb[i]     = NULL;
        menu->state[i]  = NULL;
        menu->toggle[i] = false;
    }

    menu->main = lv_obj_create(parent);
    /* Size & positioning */
    LV_SET_SIZE(menu->main, lv_pct(97), lv_pct(95));
    LV_ALIGN(menu->main, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scroll_dir(menu->main, LV_DIR_VER);
    LV_SET_PAD_TOP(menu->main, 5);
    LV_SET_PAD_BOTTOM(menu->main, 20);

    /* Vertical layout */
    LV_SET_FLEX_FLOW(menu->main, LV_FLEX_FLOW_COLUMN);
    LV_SET_FLEX_ALIGN(menu->main, LV_FLEX_ALIGN_START, /* main axis */
                      LV_FLEX_ALIGN_START,             /* cross axis */
                      LV_FLEX_ALIGN_START);            /* track align */

    /* Optional spacing between items */
    LV_SET_ROW_PAD(menu->main, 8);
    lv_obj_set_style_base_dir(menu->main, LV_BASE_DIR_RTL, 0);

    menu->selector = lv_obj_create(menu->main);
    LV_SET_BG_COLOR(menu->selector, MAIN_THEME_COLOR);
    LV_SET_BG_OPA(menu->selector, LV_OPA_20);
    lv_obj_move_background(menu->selector);
    lv_obj_add_flag(menu->selector, LV_OBJ_FLAG_IGNORE_LAYOUT);
    LV_SET_RADIUS(menu->selector, 16);
    lv_obj_set_style_anim_time(menu->selector, 300, 0);
    LV_HIDE(menu->selector);

    menu->checker = lv_label_create(menu->main);
    LV_SET_TEXT(menu->checker, LV_SYMBOL_OK); // built-in LVGL symbol
    LV_HIDE(menu->checker);
    lv_obj_add_flag(menu->checker, LV_OBJ_FLAG_IGNORE_LAYOUT);

    menu->togglable = false;
    ui_menu_hide(menu);
    return ERR_OK;
}

void ui_menu_destroy(Menu* menu) {
    RETURN_IF_NULL(menu, ;);

    if (ui_menu_validate(menu)) {
        LV_DELETE(menu->main);
    }
}

void ui_menu_on_off(Menu* menu, lv_obj_t* parent) {
    RETURN_IF_NULL(menu, ;);
    ui_menu_create(menu, parent);
    ui_menu_addItem(menu, phraseGetDef(PHRASE_ENABLE), LV_TEXT_ALIGN_RIGHT,
                    NULL, NULL, NULL);
    ui_menu_addItem(menu, phraseGetDef(PHRASE_DISABLE), LV_TEXT_ALIGN_RIGHT,
                    NULL, NULL, NULL);
}

void ui_menu_togglable(Menu* menu, lv_obj_t* parent) {
    RETURN_IF_NULL(menu, ;);
    ui_menu_create(menu, parent);
    menu->togglable = true;
}

bool ui_menu_validate(const Menu* menu) {
    return (menu && menu->main && lv_obj_is_valid(menu->main));
}