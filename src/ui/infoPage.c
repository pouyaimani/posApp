#include "infoPage.h"
#include "display/display.h"
#include "font/myFont.h"
#include "common.h"
#include "logger.h"

static void createInfoPage(InfoPage *pinfo) {
    RETURN_IF_NULL(pinfo, ;);
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
    RETURN_IF_NULL(pinfo, ;);
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
    RETURN_IF_NULL(pinfo, ;);
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
    RETURN_IF_NULL(pinfo, ;);
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
    return &info;
}