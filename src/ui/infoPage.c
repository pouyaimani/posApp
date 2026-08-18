#include "infoPage.h"
#include "display/display.h"
#include "font/myFont.h"
#include "common.h"
#include "logger.h"
#include "statusIndicator.h"

#define INFO_ICON_Y_OFFSET (-60)

static StatusIndicator indicator;

static void createInfoPage(InfoPage* pinfo) {
    RETURN_IF_NULL(pinfo, ;);
    pinfo->parent = disp()->screen;
    statusIndicatorCreate(&indicator, pinfo->parent);
    statusIndicatorAlign(&indicator, LV_ALIGN_CENTER, 0, INFO_ICON_Y_OFFSET);

    pinfo->title = lv_label_create(pinfo->parent);
    LV_SET_TEXT_FONT(pinfo->title, FONT_20);
    LV_SET_TEXT_COLOR(pinfo->title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->title, LV_ALIGN_CENTER, 0, 0);

    pinfo->body = lv_label_create(pinfo->parent);
    LV_SET_TEXT_FONT(pinfo->body, FONT_16);
    LV_SET_TEXT_COLOR(pinfo->body, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->body, LV_ALIGN_CENTER, 0, 30);

    pinfo->img = NULL;

    pinfo->line = lv_obj_create(pinfo->parent);
    LV_SET_SIZE(pinfo->line, 190, 8);
    LV_ALIGN(pinfo->line, LV_ALIGN_TOP_MID, 0, 140);
    LV_SET_RADIUS(pinfo->line, 17);
    LV_SET_BG_COLOR(pinfo->line, 0x333333);
    LV_SET_BORDER_COLOR(pinfo->line, 0x333333);

    LV_SET_TEXT(pinfo->title, "");
    LV_SET_TEXT(pinfo->body, "");

    LV_HIDE(pinfo->body);
    LV_HIDE(pinfo->title);
    LV_HIDE(pinfo->line);
}

static void infoHide(InfoPage* pinfo) {
    RETURN_IF_NULL(pinfo, ;);
    if (pinfo->img) {
        LV_HIDE(pinfo->img);
        LV_DELETE(pinfo->img);
        pinfo->img = NULL;
    }
    LV_HIDE(pinfo->body);
    LV_HIDE(pinfo->title);
    LV_HIDE(pinfo->line);
    statusIndicatorShow(&indicator, STATUS_INDICATOR_HIDDEN);
    statusIndicatorHide(&indicator);
}

static void infoShow(InfoPage* pinfo) {
    RETURN_IF_NULL(pinfo, ;);
    LOG_DEBUG("pinfo->type = %d", pinfo->type);
    if (pinfo->type == INFO_IMG) {
        LV_SHOW(pinfo->body);
        if (pinfo->img) {
            LV_SHOW(pinfo->img);
        }
        LV_SHOW(pinfo->line);
        LV_ALIGN(pinfo->body, LV_ALIGN_TOP_MID, 0, 161);
        return;
    }
    LV_SHOW(pinfo->body);
    LV_SHOW(pinfo->title);
    StatusIndicatorState st;
    switch (pinfo->type) {
    case INFO_SUCCESS:
        st = STATUS_INDICATOR_SUCCESS;
        break;
    case INFO_ERROR:
        st = STATUS_INDICATOR_ERROR;
        break;
    case INFO_WAITING:
        st = STATUS_INDICATOR_WAITING;
        break;
    case INFO_WARNING:
        st = STATUS_INDICATOR_WARNING;
        break;

    default:
        return;
        break;
    }

    statusIndicatorShow(&indicator, st);
}

static void fUpdate() { disp()->refreshNow(); }

static void infoSetData(InfoPage* pinfo, InfoType_t type, const char* data,
                        const char* body) {
    RETURN_IF_NULL(pinfo, ;);
    pinfo->type = type;
    if (type == INFO_IMG) {
        if (pinfo->img) {
            lv_obj_del(pinfo->img);
            pinfo->img = NULL;
        }
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
    // TODO: force update lvgl
}

InfoPage* infoPage() {
    static InfoPage info;
    CALL_ONCE(createInfoPage(&info); info.vtable.hide = infoHide;
              info.vtable.show = infoShow; info.vtable.setData = infoSetData;
              info.vtable.forceUpdate = fUpdate; infoHide(&info););
    return &info;
}

void SHOW_INFO(InfoType_t type, const char* title, const char* body) {
    InfoPage* info = infoPage();
    OOP_CALL(info, setData, (InfoType_t)type, title, body);
    OOP_CALL(info, show);
};

void HIDE_INFO() {
    InfoPage* info = infoPage();
    OOP_CALL(info, hide);
};