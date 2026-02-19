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
    lv_obj_scroll_to_view(menu->item[menu->idx], LV_ANIM_ON);
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

void uiMenu(Menu *menu, lv_obj_t * parent) {
    menu->vtable.addItem = addItem;
    menu->vtable.handleItem = handleItem;
    menu->vtable.show = menuShow;
    menu->vtable.hide = menuHide;
    menu->vtable.getIdx = menuGetIdx;
    menu->cnt = 0;
    menu->idx = 0;

    menu->main = lv_obj_create(parent);

    /* Size & positioning */
    LV_SET_SIZE(menu->main, lv_pct(100), lv_pct(100));
    LV_ALIGN(menu->main, LV_ALIGN_CENTER, 0, 0);

    /* Vertical layout */
    LV_SET_FLEX_FLOW(menu->main, LV_FLEX_FLOW_COLUMN);
    LV_SET_FLEX_ALIGN(menu->main,
                            LV_FLEX_ALIGN_START,   /* main axis */
                           LV_FLEX_ALIGN_START,   /* cross axis */
                           LV_FLEX_ALIGN_START);  /* track align */

    /* Optional spacing between items */
    LV_SET_ROW_PAD(menu->main, 8);
    lv_obj_set_style_base_dir(menu->main, LV_BASE_DIR_RTL, 0);
}

void uiDeleteMenu(Menu *menu) {
    if(menu->main && lv_obj_is_valid(menu->main)) {
        LV_DELETE(menu->main);
        menu->main = NULL;
        menu->cnt = 0;
        menu->idx = 0;
    }
}

static void createInfoPage(InfoPage *pinfo) {
    pinfo->title = lv_label_create(getDisplay()->screen);
    LV_SET_TEXT_FONT(pinfo->title, FONT_20);
    LV_SET_TEXT_COLOR(pinfo->title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->title, LV_ALIGN_CENTER, 0, -50);
    
    pinfo->body = lv_label_create(getDisplay()->screen);
    LV_SET_TEXT_FONT(pinfo->body, FONT_16);
    LV_SET_TEXT_COLOR(pinfo->body, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->body, LV_ALIGN_CENTER, 0, 0);
    
    pinfo->img = NULL;

    pinfo->line = lv_obj_create(getDisplay()->screen);
    LV_SET_SIZE(pinfo->line, 190, 8);
    LV_ALIGN(pinfo->line, LV_ALIGN_TOP_MID, 0, 140);
    LV_SET_RADIUS(pinfo->line, 17);
    LV_SET_BG_COLOR(pinfo->line, 0x333333);
    LV_SET_BORDER_COLOR(pinfo->line, 0x333333);

    LV_SET_TEXT(pinfo->title, "");
    LV_SET_TEXT(pinfo->body, "");
}

static void infoHide(InfoPage *pinfo) {
    LV_HIDE(pinfo->body);
    LV_HIDE(pinfo->title);
    if(pinfo->img) {
        LV_HIDE(pinfo->img);
        LV_DELETE(pinfo->img);
        pinfo->img = NULL;
    }
    LV_HIDE(pinfo->line);
}

static void infoShow(InfoPage *pinfo) {
    if (pinfo->type == INFO_T_IMG) {
        LV_SHOW(pinfo->body);
        if (pinfo->img) {
            LV_SHOW(pinfo->img);
        }
        LV_SHOW(pinfo->line);
        LV_ALIGN(pinfo->body, LV_ALIGN_TOP_MID, 0, 161);
    } else {
        LV_SHOW(pinfo->body);
        LV_ALIGN(pinfo->body, LV_ALIGN_CENTER, 0, 0);
        LV_SHOW(pinfo->title);
    }
}

static void infoSetData(InfoPage *pinfo, InfoType_t type, const char *data, const char* body) {
    pinfo->type = type;
    if (type == INFO_T_IMG) {
        pinfo->img = lv_img_create(getDisplay()->screen);
        LV_ALIGN(pinfo->img, LV_ALIGN_TOP_MID, 0, 36);
        LV_SCROLL_DISABLE(pinfo->img);
        LV_CLICK_DISABLE(pinfo->img);
        lv_img_set_src(pinfo->img, data);
        LV_SET_TEXT(pinfo->body, body);
    } else {
        LV_SET_TEXT(pinfo->title, data);
        LV_SET_TEXT(pinfo->body, body);
    }
}

InfoPage infoPage() {
    static InfoPage info;
    CALL_ONCE(
        createInfoPage(&info);
        info.vtable.hide = infoHide;
        info.vtable.show = infoShow;
        info.vtable.setData = infoSetData;
        infoHide(&info);
    );
    LV_SET_TEXT(info.title, "");
    LV_SET_TEXT(info.body, "");
    return info;
}