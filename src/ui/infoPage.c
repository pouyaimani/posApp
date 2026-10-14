#include "infoPage.h"
#include "display/display.h"
#include "font/myFont.h"
#include "common.h"
#include "logger.h"
#include "statusIndicator.h"

#define INFO_ICON_Y_OFFSET (-60)

static InfoPage        __infoPage;
static StatusIndicator indicator;

static void createInfoPage() {
    __infoPage.parent = disp()->screen;
    statusIndicatorCreate(&indicator, __infoPage.parent);
    statusIndicatorAlign(&indicator, LV_ALIGN_CENTER, 0, INFO_ICON_Y_OFFSET);

    __infoPage.title = lv_label_create(__infoPage.parent);
    LV_SET_TEXT_FONT(__infoPage.title, FONT_20);
    LV_SET_TEXT_COLOR(__infoPage.title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(__infoPage.title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(__infoPage.title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(__infoPage.title, LV_ALIGN_CENTER, 0, 0);

    __infoPage.body = lv_label_create(__infoPage.parent);
    LV_SET_TEXT_FONT(__infoPage.body, FONT_16);
    LV_SET_TEXT_COLOR(__infoPage.body, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(__infoPage.body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(__infoPage.body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(__infoPage.body, LV_ALIGN_CENTER, 0, 30);

    __infoPage.img = NULL;

    __infoPage.line = lv_obj_create(__infoPage.parent);
    LV_SET_SIZE(__infoPage.line, 190, 8);
    LV_ALIGN(__infoPage.line, LV_ALIGN_TOP_MID, 0, 140);
    LV_SET_RADIUS(__infoPage.line, 17);
    LV_SET_BG_COLOR(__infoPage.line, 0x333333);
    LV_SET_BORDER_COLOR(__infoPage.line, 0x333333);

    LV_SET_TEXT(__infoPage.title, "");
    LV_SET_TEXT(__infoPage.body, "");

    LV_HIDE(__infoPage.body);
    LV_HIDE(__infoPage.title);
    LV_HIDE(__infoPage.line);
}

static void infoHide() {
    if (__infoPage.img) {
        LV_HIDE(__infoPage.img);
        LV_DELETE(__infoPage.img);
        __infoPage.img = NULL;
    }
    LV_HIDE(__infoPage.body);
    LV_HIDE(__infoPage.title);
    LV_HIDE(__infoPage.line);
    statusIndicatorShow(&indicator, STATUS_INDICATOR_HIDDEN);
    statusIndicatorHide(&indicator);
}

static void infoShow() {
    if (__infoPage.type == INFO_IMG) {
        LV_SHOW(__infoPage.body);
        if (__infoPage.img) {
            LV_SHOW(__infoPage.img);
        }
        LV_SHOW(__infoPage.line);
        LV_ALIGN(__infoPage.body, LV_ALIGN_TOP_MID, 0, 161);
        return;
    }
    LV_SHOW(__infoPage.body);
    LV_SHOW(__infoPage.title);
    StatusIndicatorState st;
    switch (__infoPage.type) {
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

static void infoSetData(InfoType_t type, const char* data, const char* body) {
    __infoPage.type = type;
    if (type == INFO_IMG) {
        if (__infoPage.img) {
            lv_obj_del(__infoPage.img);
            __infoPage.img = NULL;
        }
        __infoPage.img = lv_img_create(disp()->screen);
        LV_ALIGN(__infoPage.img, LV_ALIGN_TOP_MID, 0, 36);
        LV_SCROLL_DISABLE(__infoPage.img);
        LV_CLICK_DISABLE(__infoPage.img);
        lv_img_set_src(__infoPage.img, data);
        LV_SET_TEXT(__infoPage.body, body);
    } else {
        LV_SET_TEXT(__infoPage.title, data);
        LV_SET_TEXT(__infoPage.body, body);
    }
    // TODO: force update lvgl
}

OOP_CTOR(InfoPage) {
    createInfoPage(self);
    self->hide        = infoHide;
    self->show        = infoShow;
    self->setData     = infoSetData;
    self->forceUpdate = fUpdate;
    infoHide(self);
}

InfoPage* infoPage() {
    CALL_ONCE(OOP_CALL_CTOR(InfoPage, &__infoPage););
    return &__infoPage;
}

void SHOW_INFO(InfoType_t type, const char* title, const char* body) {
    InfoPage* info = infoPage();
    info->setData((InfoType_t)type, title, body);
    info->show();
};

void HIDE_INFO() {
    InfoPage* info = infoPage();
    info->hide();
};