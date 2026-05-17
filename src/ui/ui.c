#include "ui.h"
#include "display.h"
#include "utility/utility.h"
#include "event.h"

#define INPUT_BOX_HEIGHT 46
#define INPUT_BOX_WIDTH 270

void uiInputBox(InputBox *inBox, lv_obj_t *parent) {
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

void uiButton(Button *btn, lv_obj_t *parent, unsigned int color, const char * text) {
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

static void createInfoPage(InfoPage *pinfo) {
    pinfo->title = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(pinfo->title, FONT_20);
    LV_SET_TEXT_COLOR(pinfo->title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->title, LV_ALIGN_CENTER, 0, -50);
    
    pinfo->body = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(pinfo->body, FONT_16);
    LV_SET_TEXT_COLOR(pinfo->body, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->body, LV_ALIGN_CENTER, 0, 0);
    
    pinfo->img = NULL;

    pinfo->line = lv_obj_create(disp()->screen);
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
        pinfo->img = lv_img_create(disp()->screen);
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

InfoPage *infoPage() {
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
    return &info;
}