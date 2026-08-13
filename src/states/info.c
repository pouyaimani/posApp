#include "states.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "utility/utility.h"
#include "font/myFont.h"
#include "myColor.h"
#include "ui/infoPage.h"
#include "sys/sys.h"
#include "phrases/phrases.h"
#include "cellular/cellular.h"
#include "settings/settings.h"

static void setData(InfoType_t type, const char* title, const char* body) {
    OOP_CALL(infoPage(), setData, type, title, body);
}

STATE_DEF_ENTER(Info) { OOP_CALL(infoPage(), show); }

STATE_DEF_EXIT(Info) {
    OOP_CALL(infoPage(), hide);
    setData(INFO_WAITING, "", "");
}

STATE_DEF_HANDLE(Info, TimeOutEvent) {}

STATE_DEF_HANDLE(Info, KeypadEvent) { SM_GOTO(state->next); }

OOP_CTOR(Info, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(Info);
    self->base.vtable.exit          = STATE_EXIT(Info);
    self->base.vtable.handleKeypad  = STATE_HANDLE(Info, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Info, TimeOutEvent);
    self->setData                   = setData;
}

/*******************************************************************/

static void rowMenuAdd(lv_obj_t* menu, const char* description,
                       const char* val) {

    // Create container
    lv_obj_t* cont = lv_obj_create(menu);
    LV_SET_SIZE(cont, lv_pct(100), LV_SIZE_CONTENT);

    // Remove default styling if needed
    LV_SET_PAD_ALL(cont, 0);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, 0);
    LV_SET_BORDER_WIDTH(cont, 1);

    // Enable horizontal flex layout
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_SPACE_BETWEEN, // main axis
                          LV_FLEX_ALIGN_CENTER,        // cross axis
                          LV_FLEX_ALIGN_CENTER);       // track cross

    // Description label (right side)
    lv_obj_t* desc = lv_label_create(cont);
    LV_SET_SIZE(desc, lv_pct(50), LV_SIZE_CONTENT);
    LV_SET_TEXT_FONT(desc, FONT_16);
    LV_SET_TEXT_ALIGN(desc, LV_TEXT_ALIGN_RIGHT);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_SCROLL);
    LV_SET_TEXT(desc, description);

    // Value label (left side)
    lv_obj_t* value = lv_label_create(cont);
    LV_SET_SIZE(value, lv_pct(50), LV_SIZE_CONTENT);
    LV_SET_TEXT_FONT(value, FONT_16);
    LV_SET_TEXT_ALIGN(value, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(value, LV_LABEL_LONG_SCROLL);

    LV_SET_TEXT(value, val);
}

lv_obj_t* uiRowMenu(lv_obj_t* parent) {
    lv_obj_t* main = lv_obj_create(parent);
    LV_SET_SIZE(main, lv_pct(97), lv_pct(100));
    LV_ALIGN(main, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scroll_dir(main, LV_DIR_VER);

    /* Vertical layout */
    LV_SET_FLEX_FLOW(main, LV_FLEX_FLOW_COLUMN);
    LV_SET_FLEX_ALIGN(main, LV_FLEX_ALIGN_START, /* main axis */
                      LV_FLEX_ALIGN_START,       /* cross axis */
                      LV_FLEX_ALIGN_START);      /* track align */

    /* Optional spacing between items */
    LV_SET_ROW_PAD(main, 0);
    lv_obj_set_style_base_dir(main, LV_BASE_DIR_RTL, 0);
    return main;
}

static void getOperatorDsc(char* op, size_t size) {
    static char* current = NULL;
    if (OOP_CALL(cellular(), getSimStatus) != SIM_STATUS_OK) {
        snprintf(op, size, "%s", phraseGetDef(PHRASE_NO_SIM));
        return;
    }
    CellSimInfo simInfo;
    if (OOP_CALL(cellular(), getSimInfo, &simInfo) != CELL_ERR_OK) {
        snprintf(op, size, "%s", phraseGetDef(PHRASE_NO_SIM));
        return;
    }
    Phrases_t      phrase;
    static uint8_t prevOpt    = 0;
    char           opt[2 + 1] = {0};
    memcpy(opt, &simInfo.imsi[3], 2);
    uint8_t opType;
    STRING_TO_U16(opt, &opType);
    getSimOpName(opType, op, size);
}

lv_obj_t* menu;

STATE_DEF_ENTER(DevInfo) {
    menu = uiRowMenu(disp()->screen);
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    rowMenuAdd(menu, phraseGetDef(PHRASE_SERIAL), sn);
    rowMenuAdd(menu, phraseGetDef(PHRASE_DEVICE_CODE),
               OOP_CALL(sys(), getCode));
    rowMenuAdd(menu, phraseGetDef(PHRASE_TERMINAL),
               settings()->terminal.terminalId);
    rowMenuAdd(menu, phraseGetDef(PHRASE_MERCHANT),
               settings()->terminal.merchantName);
    DEFINE_STRING(imei, 48);
    OOP_CALL(cellular(), getImei, imei, sizeof(imei));
    rowMenuAdd(menu, phraseGetDef(PHRASE_IMEI), imei);
    DEFINE_STRING(op, 48);
    getOperatorDsc(op, sizeof(op));
    rowMenuAdd(menu, phraseGetDef(PHRASE_OPERATOR), op);
    LV_SHOW(menu);
}

STATE_DEF_EXIT(DevInfo) {
    LV_HIDE(menu);
    LV_DELETE(menu);
}

STATE_DEF_HANDLE(DevInfo, TimeOutEvent) {}

STATE_DEF_HANDLE(DevInfo, KeypadEvent) { SM_GOTO(state->prev); }

OOP_CTOR(DevInfo, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(DevInfo);
    self->base.vtable.exit          = STATE_EXIT(DevInfo);
    self->base.vtable.handleKeypad  = STATE_HANDLE(DevInfo, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(DevInfo, TimeOutEvent);
}