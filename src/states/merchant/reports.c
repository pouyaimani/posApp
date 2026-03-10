#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "storage/storage.h"

static Storage *storage;
static Device *dev;

typedef enum {
    REP_ITEM_REPRINT = 0,
    REP_ITEM_DAILY,
    REP_ITEM_SUMMARY,
    REP_ITEM_DETAILS,
    REP_ITEM_ALL
} ReportsItem_t;

static SubState *subReports[REP_ITEM_ALL];

static const char* reportsItemTxt[REP_ITEM_ALL] = {
    "چاپ مجدد",
    "گزارش روزانه",
    "گزارش تجمیعی",
    "ریز تراکنش ها"
};

/******************** re print sub state **********************/
typedef enum {
    REPRINT_ALL = 0,
    REPRINT_SALE,
    REPRINT_BILL,
    REPRINT_CHARGE,
    REPRINT_PIN,
    REPRINT_TRACE,
    REPRINT_REF,
    REPRINT_END
} ReprintItem_t;

static SubState *startReprint;

static const char* reprintItemTxt[REPRINT_END] = {
    "همه تراکنش ها",
    "خرید",
    "پرداخت قبض",
    "شارژ مستقیم",
    "کد شارژ",
    "بر اساس پیگیری",
    "بر اساس مرجع"
};
static Menu reprintMenu;

STATE_DEF_ENTER(RePrint) {
    uiMenu(&reprintMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < REPRINT_END ; i++) {
        OOP_CALL(&reprintMenu, addItem, reprintItemTxt[i], NULL, NULL, NULL);
    }
    GOTO_MENU(getState(STATE_ID_SUPPORTER), &reprintMenu);
}

STATE_DEF_ENTER(StartRePrint) {
    SHOW_INFO("لطفا منتظر بمانید", "");
}

STATE_DEF_EXIT(StartRePrint) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(StartRePrint, KeypadEvent) {
    OOP_CALL(&reprintMenu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        InfoPage info = infoPage();
        OOP_CALL(&info, setData, INFO_T_TEXT, "نتیجه ای یافت نشد", "");
        SM_GOTO(getState(state->parent));
    }
}

static void RePrint(State *parent) {
    subReports[REP_ITEM_REPRINT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_REPRINT], parent, "re print");
    subReports[REP_ITEM_REPRINT]->vtable.enter = STATE_ENTER(RePrint);

    startReprint = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, startReprint, subReports[REP_ITEM_REPRINT], "start re print");
    startReprint->vtable.enter = STATE_ENTER(StartRePrint);
    startReprint->vtable.exit = STATE_EXIT(StartRePrint);
    startReprint->vtable.handleKeypad = STATE_HANDLE(StartRePrint, KeypadEvent);
}

/******************** daily reports sub state **********************/

STATE_DEF_ENTER(DailyReport) {

}

STATE_DEF_EXIT(DailyReport) {
}

STATE_DEF_HANDLE(DailyReport, KeypadEvent) {

}

static void DailyReport(State *parent) {
    subReports[REP_ITEM_DAILY] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_DAILY], parent, "daily reports");
    subReports[REP_ITEM_DAILY]->vtable.enter = STATE_ENTER(DailyReport);
    subReports[REP_ITEM_DAILY]->vtable.exit = STATE_EXIT(DailyReport);
}

/******************** summary report sub state **********************/

STATE_DEF_ENTER(SummaryReport) {
}

STATE_DEF_EXIT(SummaryReport) {
}

STATE_DEF_HANDLE(SummaryReport, KeypadEvent) {
}

static void SummaryReport(State *parent) {
    subReports[REP_ITEM_SUMMARY] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_SUMMARY], parent, "summary report");
    subReports[REP_ITEM_SUMMARY]->vtable.enter = STATE_ENTER(SummaryReport);
    subReports[REP_ITEM_SUMMARY]->vtable.exit = STATE_EXIT(SummaryReport);
    subReports[REP_ITEM_SUMMARY]->vtable.handleKeypad = STATE_HANDLE(SummaryReport, KeypadEvent);
}

/******************** detail report sub state **********************/

STATE_DEF_ENTER(DetailsReport) {
}

STATE_DEF_EXIT(DetailsReport) {
}

STATE_DEF_HANDLE(DetailsReport, KeypadEvent) {
}

static void DetailsReport(State *parent) {
    subReports[REP_ITEM_DETAILS] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_DETAILS], parent, "details reprt");
    subReports[REP_ITEM_DETAILS]->vtable.enter = STATE_ENTER(DetailsReport);
    subReports[REP_ITEM_DETAILS]->vtable.exit = STATE_EXIT(DetailsReport);
    subReports[REP_ITEM_DETAILS]->vtable.handleKeypad = STATE_HANDLE(DetailsReport, KeypadEvent);
}

/******************** Settings sub state **********************/
static Menu reportsMenu;

static void createUi() {
    uiMenu(&reportsMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < REP_ITEM_ALL ; i++) {
        OOP_CALL(&reportsMenu, addItem, reportsItemTxt[i], subReports[i], NULL, NULL);
    }
}

STATE_DEF_ENTER(Reports) {
    createUi();
    GOTO_MENU(state->parent, &reportsMenu);
}

OOP_CTOR(Reports, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Reports);
    storage = getStorage();
    dev = getDevice();

    RePrint(self);
    DailyReport(self);
    SummaryReport(self);
    DetailsReport(self);
}