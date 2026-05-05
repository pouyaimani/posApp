#include "merchant.h"
#include "states/states.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "storage/storage.h"
#include "utility/utility.h"
#include "settings/settings.h"
#include "record/shift/shifts.h"
#include "logger.h"

static Device *dev;

static Menu shiftItemMenu;
static lv_obj_t *shiftMenu;

static TerminalSettings *terminalStg;

typedef enum {
    SHIFT_ITEM_ENABLE = 0,
    SHIFT_ITEM_SHOW_CURRENT,
    SHIFT_ITEM_CREATE,
    SHIFT_ITEM_CLOSE,
    SHIFT_ITEM_REPORT,
    SHIFT_ITEM_ALL
} ReportsItem_t;

static SubState *subShift[SHIFT_ITEM_ALL];

static const char* shiftItemTxt[SHIFT_ITEM_ALL] = {
    "فعالسازی شیفت",
    "نمایش شیفت جاری",
    "ایجاد شیفت",
    "بستن شیفت",
    "گزارش شیفت"
};

static Menu EnMenu;

static void ShiftMenuAdd(lv_obj_t *menu, 
    const char *description, const char *val, lv_text_align_t txtAlign) {

    // Create container
    lv_obj_t * cont = lv_obj_create(menu);
    LV_SET_SIZE(cont, lv_pct(100), lv_pct(33));

    // Remove default styling if needed
    LV_SET_PAD_ALL(cont, 3);

    // Enable horizontal flex layout
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,  // main axis
                          LV_FLEX_ALIGN_CENTER,         // cross axis
                          LV_FLEX_ALIGN_CENTER);        // track cross

    // Description label (right side)
    lv_obj_t * desc = lv_label_create(cont);
    LV_SET_SIZE(desc, lv_pct(50), LV_SIZE_CONTENT);
    LV_SET_TEXT_FONT(desc, FONT_16);
    LV_SET_TEXT_ALIGN(desc, LV_TEXT_ALIGN_RIGHT);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_SCROLL);
    LV_SET_TEXT(desc, description);

    // Value label (left side)
    lv_obj_t * value = lv_label_create(cont);
    LV_SET_SIZE(value, lv_pct(50), LV_SIZE_CONTENT);
    LV_SET_TEXT_FONT(value, FONT_16);
    LV_SET_TEXT_ALIGN(value, txtAlign);
    lv_label_set_long_mode(value, LV_LABEL_LONG_SCROLL);

    LV_SET_TEXT(value, val);
}

lv_obj_t *createShiftMenu(lv_obj_t * parent) {
    lv_obj_t *main = lv_obj_create(parent);
    LV_SET_SIZE(main, lv_pct(90), lv_pct(70));
    LV_ALIGN(main, LV_ALIGN_CENTER, 0, 0);
    LV_SET_BORDER_WIDTH(main, 5);
    LV_SET_BORDER_COLOR(main, MAIN_THEME_COLOR);
    LV_SET_RADIUS(main, 8);
    LV_SET_BORDER_OPA(main, LV_OPA_COVER);

    /* Vertical layout */
    LV_SET_FLEX_FLOW(main, LV_FLEX_FLOW_COLUMN);
    LV_SET_FLEX_ALIGN(main,
                            LV_FLEX_ALIGN_START,   /* main axis */
                           LV_FLEX_ALIGN_START,   /* cross axis */
                           LV_FLEX_ALIGN_START);  /* track align */
    lv_obj_set_style_base_dir(main, LV_BASE_DIR_RTL, 0);
    return main;
}

static void showShift(lv_obj_t *menu, int latest, const char *sdt,
    const char *edt, lv_text_align_t sAlign, lv_text_align_t eAlign) {
    char strNum[6];
    intToStr(latest + 1, strNum, 6);
    ShiftMenuAdd(menu, "شماره شیفت  ", strNum, LV_TEXT_ALIGN_CENTER);
    ShiftMenuAdd(menu, "زمان شروع  ", sdt, sAlign);
    ShiftMenuAdd(menu, "زمان پایان  ", edt, eAlign);
}

static void dateTimeToInt(uint32_t *date, uint32_t *time) {
    DateTime *dt = OOP_CALL(getDevice(), getDateTime);
    Date_t jd = getJalaliDate();
    int hh, mm, ss;
    sscanf(dt->time, "%2d%2d%2d", &hh, &mm, &ss);
    *date = jd.year * 10000 + 
            jd.month * 100 + jd.day;
    *time = hh * 10000 + 
            mm * 100 + ss;
}

static void dateTimeToStr(uint32_t date, uint32_t time, char *str, size_t size) {
    char dt[24] = {0};
    int yy = date / 10000;
    int tmp = (date % 10000);
    int mm = tmp / 100;
    int dd = tmp % 100;

    int hh = time / 10000;
    tmp = (time % 10000);
    int min = tmp / 100;
    int ss = tmp % 100;
    snprintf(str, size, "%02d:%02d:%02d-%02d/%02d/%04d", ss, min, hh, dd, mm, yy);
}

/******************** En/Dis shift sub state **********************/

STATE_DEF_ENTER(ShiftEnable) {
    uiOnOffMenu(&EnMenu, getDisplay()->screen);
    OOP_CALL(&EnMenu, setChecked, !terminalStg->shiftEnable);
    OOP_CALL(&EnMenu, show);
}

STATE_DEF_EXIT(ShiftEnable) {
    OOP_CALL(&EnMenu, hide);
    uiDeleteMenu(&EnMenu);
    settings()->save();
}

STATE_DEF_HANDLE(ShiftEnable, KeypadEvent) {
    OOP_CALL(&EnMenu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        if (EnMenu.idx) {
            if (terminalStg->shiftActive) {
                GOTO_INFO(state->parent, state->parent, "شیفت فعال می باشد", "ابتدا شیفت فعال را ببندید");
                return;
            }
        }
        OOP_CALL(&EnMenu, setChecked, EnMenu.idx);
        terminalStg->shiftEnable = !EnMenu.idx;
    }
}

/******************** Show current shift sub state **********************/

STATE_DEF_ENTER(ShowCurrentShift) {
    if (terminalStg->shiftActive) {
        uint32_t idx = shifts()->getLatestIdx();
        ShiftData data;
        if (shifts()->getKeeped(&data) != 0) {
            return;
        }

        uint32_t sdate = data.startDate;
        uint32_t stime = data.startTime;
        char dt[24] = {0};
        shiftMenu = createShiftMenu(getDisplay()->screen);
        dateTimeToStr(sdate, stime, dt, sizeof(dt));
        showShift(shiftMenu, idx, dt, "...", LV_TEXT_ALIGN_LEFT, LV_TEXT_ALIGN_CENTER);
        LV_SHOW(shiftMenu);
    } else {
        GOTO_INFO(state->parent, state->parent, "شیفت فعالی یافت نشد", "");
    }
}

STATE_DEF_EXIT(ShowCurrentShift) {
    if(lv_obj_is_valid(shiftMenu)) {
        LV_HIDE(shiftMenu);
        LV_DELETE(shiftMenu);
    }
}

STATE_DEF_HANDLE(ShowCurrentShift, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    }
}

/******************** Create shift sub state **********************/

static uint32_t sdate, stime;

STATE_DEF_ENTER(CreateShift) {
    if (!terminalStg->shiftEnable) {
        GOTO_INFO(state->parent, state->parent, "شیفت غیر فعال می باشد", "ابتدا شیفت را فعال کنید");
        return;
    }
    if (!terminalStg->shiftActive) {
        uint16_t idx = shifts()->getLatestIdx();
        shiftMenu = createShiftMenu(getDisplay()->screen);
        char sdt[24];
        dateTimeToInt(&sdate, &stime);
        dateTimeToStr(sdate, stime, sdt, sizeof(sdt));
        showShift(shiftMenu, idx, sdt, "...", LV_TEXT_ALIGN_LEFT, LV_TEXT_ALIGN_CENTER);
        LV_SHOW(shiftMenu);
    } else {
        GOTO_INFO(state->parent, state->parent, "شیفت در حال اجرا می باشد", "");
    }
}

STATE_DEF_EXIT(CreateShift) {
    if(lv_obj_is_valid(shiftMenu)) {
        LV_HIDE(shiftMenu);
        LV_DELETE(shiftMenu);
    }
}

STATE_DEF_HANDLE(CreateShift, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else {
        GOTO_INFO(state->parent, state->parent, "شیفت با موفقیت فعال شد", "");
        terminalStg->shiftActive = true;
        ShiftData data;
        data.startDate = sdate;
        data.startTime = stime;
        shifts()->keep(&data);
    }
}

/******************** Close shift sub state **********************/

static uint32_t edate, etime;

STATE_DEF_ENTER(CloseShift) {
    if (terminalStg->shiftActive) {
        uint16_t idx = shifts()->getLatestIdx();
        ShiftData data;
        shifts()->getKeeped(&data);
        dateTimeToInt(&edate, &etime);
        char sdt[24] = {0};
        char edt[24] = {0};
        shiftMenu = createShiftMenu(getDisplay()->screen);
        dateTimeToStr(data.startDate, data.startTime, sdt, sizeof(sdt));
        dateTimeToStr(edate, etime, edt, sizeof(edt));
        showShift(shiftMenu, idx, sdt, edt, LV_TEXT_ALIGN_LEFT, LV_TEXT_ALIGN_LEFT);
        LV_SHOW(shiftMenu);
    } else {
        GOTO_INFO(state->parent, state->parent, "شیفت فعالی یافت نشد", "");
    }
}

STATE_DEF_EXIT(CloseShift) {
    if(lv_obj_is_valid(shiftMenu)) {
        LV_HIDE(shiftMenu);
        LV_DELETE(shiftMenu);
    }
}

STATE_DEF_HANDLE(CloseShift, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else {
        ShiftData data;
        shifts()->getKeeped(&data);
        data.endDate = edate;
        data.endTime = etime;
        terminalStg->shiftActive = false;
        if (shifts()->insert(&data) == 0) {
            GOTO_INFO(state->parent, state->parent, "شیفت با موفقیت بسته شد", "");
        } else {
            GOTO_INFO(state->parent, state->parent, "خطا در بستن شیفت", "");
        }
    }
}

/******************** Shift report sub state **********************/

static SubState *handleReports;


STATE_DEF_ENTER(HandleReports) {
    Input *in = getState(STATE_ID_INPUT);
    int shiftNum = libAtoi(in->input);
    ShiftData data;
    if (shifts()->get(shiftNum, &data) != 0) {
        GOTO_INFO(state->parent, state->parent, "شیفت مورد نظر یافت نشد", "");
        return;
    }
    char sdt[24] = {0};
    char edt[24] = {0};
    shiftMenu = createShiftMenu(getDisplay()->screen);
    LOG_DEBUG("shift: idx = %d, startTime = %d, endTime = %d, startDate = %d, endDate = %d",
            shiftNum, data.startTime, data.endTime, data.startDate, data.endDate);
    dateTimeToStr(data.startDate, data.startTime, sdt, sizeof(sdt));
    dateTimeToStr(data.endDate, data.endTime, edt, sizeof(edt));
    showShift(shiftMenu, shiftNum - 1, sdt, edt, LV_TEXT_ALIGN_LEFT, LV_TEXT_ALIGN_LEFT);
    LV_SHOW(shiftMenu);
}

STATE_DEF_EXIT(HandleReports) {
    if(lv_obj_is_valid(shiftMenu)) {
        LV_HIDE(shiftMenu);
        LV_DELETE(shiftMenu);
    }
}

STATE_DEF_HANDLE(HandleReports, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else {
        SM_GOTO(state->parent);
    }
}

STATE_DEF_ENTER(ShiftReports) {
    GOTO_INPUT(state->parent, handleReports, "انتخاب شیفت", "", 3, IN_MODE_NUMBERS, NULL);
}

/******************** Shift settings state **********************/

static void createUi() {
    uiMenu(&shiftItemMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < SHIFT_ITEM_ALL ; i++) {
        OOP_CALL(&shiftItemMenu, addItem, shiftItemTxt[i], subShift[i], NULL, NULL);
    }
}

STATE_DEF_ENTER(Shift) {
    createUi();
    GOTO_MENU(state->parent, &shiftItemMenu, NULL, NULL);
}

OOP_CTOR(Shift, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Shift);
    dev = getDevice();
    terminalStg = &settings()->terminal;

    subShift[SHIFT_ITEM_ENABLE] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subShift[SHIFT_ITEM_ENABLE], self, "en/dis shift");
    subShift[SHIFT_ITEM_ENABLE]->vtable.enter = STATE_ENTER(ShiftEnable);
    subShift[SHIFT_ITEM_ENABLE]->vtable.exit = STATE_EXIT(ShiftEnable);
    subShift[SHIFT_ITEM_ENABLE]->vtable.handleKeypad = STATE_HANDLE(ShiftEnable, KeypadEvent);

    subShift[SHIFT_ITEM_SHOW_CURRENT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subShift[SHIFT_ITEM_SHOW_CURRENT], self, "show current shift");
    subShift[SHIFT_ITEM_SHOW_CURRENT]->vtable.enter = STATE_ENTER(ShowCurrentShift);
    subShift[SHIFT_ITEM_SHOW_CURRENT]->vtable.exit = STATE_EXIT(ShowCurrentShift);
    subShift[SHIFT_ITEM_SHOW_CURRENT]->vtable.handleKeypad = STATE_HANDLE(ShowCurrentShift, KeypadEvent);

    subShift[SHIFT_ITEM_CREATE] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subShift[SHIFT_ITEM_CREATE], self, "create shift");
    subShift[SHIFT_ITEM_CREATE]->vtable.enter = STATE_ENTER(CreateShift);
    subShift[SHIFT_ITEM_CREATE]->vtable.exit = STATE_EXIT(CreateShift);
    subShift[SHIFT_ITEM_CREATE]->vtable.handleKeypad = STATE_HANDLE(CreateShift, KeypadEvent);

    subShift[SHIFT_ITEM_CLOSE] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subShift[SHIFT_ITEM_CLOSE], self, "close shift");
    subShift[SHIFT_ITEM_CLOSE]->vtable.enter = STATE_ENTER(CloseShift);
    subShift[SHIFT_ITEM_CLOSE]->vtable.exit = STATE_EXIT(CloseShift);
    subShift[SHIFT_ITEM_CLOSE]->vtable.handleKeypad = STATE_HANDLE(CloseShift, KeypadEvent);

    subShift[SHIFT_ITEM_REPORT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subShift[SHIFT_ITEM_REPORT], self, "Shift reports");
    subShift[SHIFT_ITEM_REPORT]->vtable.enter = STATE_ENTER(ShiftReports);

    handleReports = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, handleReports, self, "Shift reports");
    handleReports->vtable.enter = STATE_ENTER(HandleReports);
    handleReports->vtable.exit = STATE_EXIT(HandleReports);
    handleReports->vtable.handleKeypad = STATE_HANDLE(HandleReports, KeypadEvent);
}