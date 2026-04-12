#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "storage/storage.h"

static Device *dev;

typedef enum {
    REP_ITEM_REPRINT = 0,
    REP_ITEM_DAILY,
    REP_ITEM_SUMMARY,
    REP_ITEM_DETAILS,
    REP_ITEM_ALL
} ReportsItem_t;

static ReportsItem_t rItem;

static SubState *subReports[REP_ITEM_ALL];
static SubState *extractData;
static SubState *getStartDate;
static SubState *getStartTime;
static SubState *getEndDate;
static SubState *getEndTime;

static State *prev;
static State *mainMenu;

static char startDate[MAX_DATE_IN_LEN + 1];
static char endDate[MAX_DATE_IN_LEN + 1];
static char startTime[MAX_TIME_IN_LEN + 1];
static char endTime[MAX_TIME_IN_LEN + 1];
static char refNum[MAX_REF_NUM_IN_LEN + 1];
static char trace[MAX_TRACE_IN_LEN + 1];

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
} PrintItem_t;

static PrintItem_t pItem;

void setReprintItem(void *arg) {
    pItem = (PrintItem_t)(uintptr_t)arg;
    switch (pItem) {
    case REPRINT_TRACE:
        GOTO_INPUT(subReports[REP_ITEM_REPRINT], extractData,
                 "شماره پیگیری را وارد کنید", "", MAX_TRACE_IN_LEN, IN_MODE_NUMBERS, trace);
        break;
    case REPRINT_REF:
        GOTO_INPUT(subReports[REP_ITEM_REPRINT], extractData,
                 "شماره مرجع را وارد کنید", "", MAX_REF_NUM_IN_LEN, IN_MODE_NUMBERS, refNum);
        break;
    default:
    if (rItem != REP_ITEM_DETAILS) {
        SM_GOTO(extractData);
    } else {
        SM_GOTO(getStartDate);
    }
        break;
    }
}

static SubState *startReprint;

static const char* printItemTxt[REPRINT_END] = {
    "همه تراکنش ها",
    "خرید",
    "پرداخت قبض",
    "شارژ مستقیم",
    "کد شارژ",
    "بر اساس پیگیری",
    "بر اساس مرجع"
};
static Menu printMenu;

STATE_DEF_ENTER(RePrint) {
    uiMenu(&printMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < REPRINT_END ; i++) {
        OOP_CALL(&printMenu, addItem, printItemTxt[i], NULL,
                    setReprintItem, (void*)(uintptr_t)i);
    }
    GOTO_MENU(state->parent, &printMenu, NULL, NULL);
}

static void RePrint(State *parent) {
    subReports[REP_ITEM_REPRINT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_REPRINT], parent, "reprint");
    subReports[REP_ITEM_REPRINT]->vtable.enter = STATE_ENTER(RePrint);
}

/******************** daily reports sub state **********************/

STATE_DEF_ENTER(DailyReport) {
    SM_GOTO(extractData);
}

static void DailyReport(State *parent) {
    subReports[REP_ITEM_DAILY] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_DAILY], parent, "daily reports");
    subReports[REP_ITEM_DAILY]->vtable.enter = STATE_ENTER(DailyReport);
}

/******************** summary report sub state **********************/

STATE_DEF_ENTER(SummaryReport) {
    SM_GOTO(getStartDate);
}

static void SummaryReport(State *parent) {
    subReports[REP_ITEM_SUMMARY] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_SUMMARY], parent, "summary report");
    subReports[REP_ITEM_SUMMARY]->vtable.enter = STATE_ENTER(SummaryReport);
}

/******************** detail report sub state **********************/

STATE_DEF_ENTER(DetailsReport) {
    uiMenu(&printMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < REPRINT_TRACE ; i++) {
        OOP_CALL(&printMenu, addItem, printItemTxt[i], NULL,
                    setReprintItem, (void*)(uintptr_t)i);
    }
    GOTO_MENU(state->parent, &printMenu, NULL, NULL);
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

/******************** Get Start Date sub state **********************/

STATE_DEF_ENTER(GetStartDate) {
    GOTO_INPUT(mainMenu, getStartTime, "از تاریخ", "", MAX_DATE_IN_LEN, IN_MODE_DATE, startDate);
}

/******************** Get End Date sub state **********************/

STATE_DEF_ENTER(GetEndDate) {
    GOTO_INPUT(mainMenu, getEndTime, "تا تاریخ", "", MAX_DATE_IN_LEN, IN_MODE_DATE, endDate);
}

/******************** Get Start Time sub state **********************/

STATE_DEF_ENTER(GetStartTime) {
    GOTO_INPUT(mainMenu, getEndDate, "از ساعت", "", MAX_TIME_IN_LEN, IN_MODE_TIME, startTime);
}

/******************** Get End Time sub state **********************/

STATE_DEF_ENTER(GetEndTime) {
    GOTO_INPUT(mainMenu, extractData, "تا ساعت", "", MAX_TIME_IN_LEN, IN_MODE_TIME, endTime);
}

/******************** extract data sub state **********************/

STATE_DEF_ENTER(ExtractData) {
    SHOW_INFO("در حال استخراج اطلاعات"," لطفا منتظر بمانید");
    LOG_DEBUG("start date = %s", startDate);
    LOG_DEBUG("start time = %s", startTime);
    LOG_DEBUG("end date = %s", endDate);
    LOG_DEBUG("end time = %s", endTime);
}

STATE_DEF_EXIT(ExtractData) {
}

STATE_DEF_HANDLE(ExtractData, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        GOTO_INFO(prev, prev, "نتیجه ای یافت نشد", "");
    }
}

static void ExtractData(State *parent) {
    extractData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, extractData, parent, "extract data");
    extractData->vtable.enter = STATE_ENTER(ExtractData);
    extractData->vtable.exit = STATE_EXIT(ExtractData);
    extractData->vtable.handleKeypad = STATE_HANDLE(ExtractData, KeypadEvent);
}

/******************** Settings sub state **********************/
static Menu reportsMenu;

static void setReportItem(void *arg) {
    rItem = (PrintItem_t)(uintptr_t)arg;
    prev = subReports[rItem];
}

STATE_DEF_ENTER(Reports) {
    uiMenu(&reportsMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < REP_ITEM_ALL ; i++) {
        OOP_CALL(&reportsMenu, addItem, reportsItemTxt[i], subReports[i], setReportItem, (void*)(uintptr_t)i);
    }
    GOTO_MENU(state->parent, &reportsMenu, NULL, NULL);
}

OOP_CTOR(Reports, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    mainMenu = self;
    self->base.vtable.enter = STATE_ENTER(Reports);
    dev = getDevice();

    RePrint(self);
    DailyReport(self);
    SummaryReport(self);
    DetailsReport(self);
    ExtractData(self);

    getStartDate = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, getStartDate, parent, "get Start Date");
    getStartDate->vtable.enter = STATE_ENTER(GetStartDate);

    getStartTime = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, getStartTime, parent, "get Start Time");
    getStartTime->vtable.enter = STATE_ENTER(GetStartTime);

    getEndDate = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, getEndDate, parent, "get End Date");
    getEndDate->vtable.enter = STATE_ENTER(GetEndDate);

    getEndTime = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, getEndTime, parent, "get End Time");
    getEndTime->vtable.enter = STATE_ENTER(GetEndTime);
}