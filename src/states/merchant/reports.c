#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/menu.h"
#include "sys/sys.h"
#include "storage/storage.h"
#include "record/txnRecs.h"
#include "utility/utility.h"
#include "receipt/receiptTemplates.h"
#include "printer/printer.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "input/inputMgr.h"

static ReceiptDocType docType;

typedef enum {
    REP_FILTER_DATE_TIME = (1u << 0),
    REP_FILTER_REF_NUM   = (1u << 1),
    REP_FILTER_TRACE     = (1u << 2),
    REP_FILTER_SERVICE   = (1u << 3),
} ReportsFilter_t;

typedef enum {
    REP_RESULT_DETAIL     = 0,
    REP_RESULT_AGGREGATED = 1,
    REP_RESULT_NORM
} ReportResultMode_t;

#define QUERY_FILTER_RESET(x) (x = 0)

#define QUERY_IS_USING_DATE_TIME(x)  ((x & REP_FILTER_DATE_TIME) > 0 ? 1 : 0)
#define QUERY_IS_USING_REF_NUM(x)    ((x & REP_FILTER_REF_NUM) > 0 ? 1 : 0)
#define QUERY_IS_USING_TRACE(x)      ((x & REP_FILTER_TRACE) > 0 ? 1 : 0)
#define QUERY_IS_USING_SERVICE_ID(x) ((x & REP_FILTER_SERVICE) > 0 ? 1 : 0)

#define QUERY_FILTER_DATE_TIME(x)  (x |= REP_FILTER_DATE_TIME)
#define QUERY_FILTER_REF_NUM(x)    (x |= REP_FILTER_REF_NUM)
#define QUERY_FILTER_TRACE(x)      (x |= REP_FILTER_TRACE)
#define QUERY_FILTER_SERVICE_ID(x) (x |= REP_FILTER_SERVICE)

#define REPORT_RESULT_DETAIL(x)     (x = REP_RESULT_DETAIL)
#define REPORT_RESULT_AGGREGATED(x) (x = REP_RESULT_DETAIL)
#define REPORT_RESULT_NORM(x)       (x = REP_RESULT_NORM)

#define REPORT_RESULT_IS_DETAIL(x)     (x == REP_RESULT_DETAIL)
#define REPORT_RESULT_IS_AGGREGATED(x) (x == REP_RESULT_DETAIL)
#define REPORT_RESULT_IS_NORM(x)       (x == REP_RESULT_NORM)

typedef enum {
    REP_ITEM_REPRINT = 0,
    REP_ITEM_DAILY,
    REP_ITEM_SUMMARY,
    REP_ITEM_DETAILS,
    REP_ITEM_ALL
} ReportsItem_t;

static ReportsItem_t rItem;

static SubState* subReports[REP_ITEM_ALL];
static SubState* extractData;
static SubState* getStartDate;
static SubState* getStartTime;
static SubState* getEndDate;
static SubState* getEndTime;

static State* prev;
static State* mainMenu;

typedef struct {
    ReportsFilter_t    filter;
    ReportResultMode_t resMode;
    char               startDate[16];
    char               endDate[16];
    char               startTime[16];
    char               endTime[16];
    char               refNum[16];
    char               trace[16];
    uint8_t            serviceId;
} ReportQuery_t;

ReportQuery_t rquery;

static const Phrases_t reportsItemTxt[REP_ITEM_ALL] = {
    PHRASE_REPRINT, PHRASE_DAILY_REPORT, PHRASE_SUMMARY_REPORT,
    PHRASE_TRANACTION_DETAILS};

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

void setReprintItem(void* arg) {
    pItem = (PrintItem_t)(uintptr_t)arg;
    switch (pItem) {
    case REPRINT_TRACE:
        QUERY_FILTER_TRACE(rquery.filter);
        inmgr()->run(
            &(InputCfg){
                .type   = INPUT_TYPE_KEYPAD,
                .mode   = INMD_ENTER_NUMBERS,
                .title  = phraseGetDef(PHRASE_ENTER_TRACE),
                .info   = "",
                .maxLen = LEN_MAX_TRACE_IN,
            },
            subReports[REP_ITEM_REPRINT], extractData);
        inmgr()->setOut(rquery.trace, NULL, sizeof(rquery.trace));
        // GOTO_INPUT(subReports[REP_ITEM_REPRINT], extractData,
        //            phraseGetDef(PHRASE_ENTER_TRACE), "", LEN_MAX_TRACE_IN,
        //            IN_MODE_NUMBERS, rquery.trace);
        break;
    case REPRINT_REF:
        QUERY_FILTER_REF_NUM(rquery.filter);
        inmgr()->run(
            &(InputCfg){
                .type   = INPUT_TYPE_KEYPAD,
                .mode   = INMD_ENTER_NUMBERS,
                .title  = phraseGetDef(PHRASE_ENTER_REF_NUM),
                .info   = "",
                .maxLen = LEN_MAX_REF_NUM_IN,
            },
            subReports[REP_ITEM_REPRINT], extractData);
        inmgr()->setOut(rquery.refNum, NULL, sizeof(rquery.refNum));
        // GOTO_INPUT(subReports[REP_ITEM_REPRINT], extractData,
        //            phraseGetDef(PHRASE_ENTER_REF_NUM), "",
        //            LEN_MAX_REF_NUM_IN, IN_MODE_NUMBERS, rquery.refNum);
        break;
    case REPRINT_BILL:
    case REPRINT_CHARGE:
    case REPRINT_SALE:
    case REPRINT_PIN:
        QUERY_FILTER_SERVICE_ID(rquery.filter);
        SM_GOTO(extractData);
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

static SubState* startReprint;

static const Phrases_t printItemTxt[REPRINT_END] = {
    PHRASE_TXN_ALL,         PHRASE_TXN_SALE,        PHRASE_TXN_BILL,
    PHRASE_TXN_TOPUP,       PHRASE_TXN_CHARGE_CODE, PHRASE_BASED_ON_TRACE,
    PHRASE_BASED_ON_REF_NUM};

static Menu* printMenu;

STATE_DEF_ENTER(RePrint) {
    REPORT_RESULT_NORM(rquery.resMode);
    ui_menu_create(printMenu, disp()->screen);
    for (uint8_t i = 0; i < REPRINT_END; i++) {
        ui_menu_addItem(printMenu, phraseGetDef(printItemTxt[i]), NULL,
                        setReprintItem, (void*)(uintptr_t)i);
    }
    GOTO_MENU(state->parent, printMenu, NULL, NULL);
}

static void RePrint(State* parent) {
    subReports[REP_ITEM_REPRINT] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_REPRINT], parent, "reprint");
    subReports[REP_ITEM_REPRINT]->vtable.enter = STATE_ENTER(RePrint);
}

/******************** daily reports sub state **********************/

STATE_DEF_ENTER(DailyReport) {
    docType = DOC_DAILY_REPORT;
    SM_GOTO(extractData);
}

static void DailyReport(State* parent) {
    subReports[REP_ITEM_DAILY] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_DAILY], parent, "daily reports");
    subReports[REP_ITEM_DAILY]->vtable.enter = STATE_ENTER(DailyReport);
}

/******************** summary report sub state **********************/

STATE_DEF_ENTER(SummaryReport) {
    docType = DOC_SUMMARY_REPORT;
    SM_GOTO(getStartDate);
}

static void SummaryReport(State* parent) {
    subReports[REP_ITEM_SUMMARY] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_SUMMARY], parent,
                  "summary report");
    subReports[REP_ITEM_SUMMARY]->vtable.enter = STATE_ENTER(SummaryReport);
}

/******************** detail report sub state **********************/

STATE_DEF_ENTER(DetailsReport) {
    docType = DOC_DETAILED_REPORT;
    ui_menu_create(printMenu, disp()->screen);
    for (uint8_t i = 0; i < REPRINT_TRACE; i++) {
        ui_menu_addItem(printMenu, printItemTxt[i], NULL, setReprintItem,
                        (void*)(uintptr_t)i);
    }
    GOTO_MENU(state->parent, printMenu, NULL, NULL);
}

STATE_DEF_EXIT(DetailsReport) {}

STATE_DEF_HANDLE(DetailsReport, KeypadEvent) {}

static void DetailsReport(State* parent) {
    subReports[REP_ITEM_DETAILS] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_DETAILS], parent, "details reprt");
    subReports[REP_ITEM_DETAILS]->vtable.enter = STATE_ENTER(DetailsReport);
    subReports[REP_ITEM_DETAILS]->vtable.exit  = STATE_EXIT(DetailsReport);
    subReports[REP_ITEM_DETAILS]->vtable.handleKeypad =
        STATE_HANDLE(DetailsReport, KeypadEvent);
}

/******************** Get Start Date sub state **********************/

STATE_DEF_ENTER(GetStartDate) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_DATE,
            .title  = phraseGetDef(PHRASE_FROM_DATE),
            .info   = "",
            .maxLen = LEN_MAX_DATE_IN,
        },
        mainMenu, getStartTime);
    inmgr()->setOut(rquery.startDate, NULL, sizeof(rquery.startDate));
    // GOTO_INPUT(mainMenu, getStartTime, phraseGetDef(PHRASE_FROM_DATE), "",
    //            LEN_MAX_DATE_IN, IN_MODE_DATE, rquery.startDate);
}

/******************** Get End Date sub state **********************/

STATE_DEF_ENTER(GetEndDate) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_DATE,
            .title  = phraseGetDef(PHRASE_TO_DATE),
            .info   = "",
            .maxLen = LEN_MAX_DATE_IN,
        },
        mainMenu, getEndTime);
    inmgr()->setOut(rquery.endDate, NULL, sizeof(rquery.endDate));
    // GOTO_INPUT(mainMenu, getEndTime, phraseGetDef(PHRASE_TO_DATE), "",
    //            LEN_MAX_DATE_IN, IN_MODE_DATE, rquery.endDate);
}

/******************** Get Start Time sub state **********************/

STATE_DEF_ENTER(GetStartTime) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_TIME,
            .title  = phraseGetDef(PHRASE_FROM_TIME),
            .info   = "",
            .maxLen = LEN_MAX_TIME_IN,
        },
        mainMenu, getEndDate);
    inmgr()->setOut(rquery.startTime, NULL, sizeof(rquery.startTime));
    // GOTO_INPUT(mainMenu, getEndDate, phraseGetDef(PHRASE_FROM_TIME), "",
    //            LEN_MAX_TIME_IN, IN_MODE_TIME, rquery.startTime);
}

/******************** Get End Time sub state **********************/

STATE_DEF_ENTER(GetEndTime) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_TIME,
            .title  = phraseGetDef(PHRASE_TO_TIME),
            .info   = "",
            .maxLen = LEN_MAX_TIME_IN,
        },
        mainMenu, extractData);
    inmgr()->setOut(rquery.endTime, NULL, sizeof(rquery.endTime));
    // GOTO_INPUT(mainMenu, extractData, phraseGetDef(PHRASE_TO_TIME), "",
    //            LEN_MAX_TIME_IN, IN_MODE_TIME, rquery.endTime);
}

/******************** extract data sub state **********************/

static bool handleExtractedData(const TxnData* txn, void* userData) {
    ReceiptData* data = (ReceiptData*)userData;
    data->txn         = txn;
    Receipt  rec;
    Result_t res = buildReceipt(&rec, data);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    res = OOP_CALL(&rec, flush);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    OOP_CALL(&rec, destroy);
    return true;
}

static int8_t checkPrinterStatus() {
    PrinterStatus_t st = OOP_CALL(printer(), getStatus);
    if (st == PRNT_STAT_READY) {
        return ERR_OK;
    }
    if (st == PRNT_STAT_NO_PAPER) {

    } else if (st == PRNT_STAT_OVER_HEAT) {

    } else {
    }
}

STATE_DEF_ENTER(ExtractData) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_EXTRACTING_DATA),
              phraseGetDef(PHRASE_PLEASE_WAIT));
    LOG_DEBUG("start date = %s", rquery.startDate);
    LOG_DEBUG("start time = %s", rquery.startTime);
    LOG_DEBUG("end date = %s", rquery.endDate);
    LOG_DEBUG("end time = %s", rquery.endTime);
    ReceiptData recData;
    recData.headerApplied = false;
    recData.type          = docType;
    switch (docType) {
    case DOC_TXN:
        break;
    case DOC_SUMMARY_REPORT:
        if (QUERY_IS_USING_DATE_TIME(rquery.filter)) {
            // TODO
            recData.summaryHeader.dateFrom = libAtoi(rquery.startDate);
            recData.summaryHeader.dateTo   = libAtoi(rquery.endDate);
            recData.summaryHeader.timeFrom = libAtoi(rquery.startTime);
            recData.summaryHeader.timeTo   = libAtoi(rquery.endDate);
            recData.summaryHeader.dateNow  = OOP_CALL(sys(), getDate);
            recData.summaryHeader.timeNow  = OOP_CALL(sys(), getTime);
            recData.summaryHeader.txnType  = 0;
        }
        break;
    case DOC_DAILY_REPORT:
        if (QUERY_IS_USING_DATE_TIME(rquery.filter)) {
            // TODO
            recData.dailyHeader.date    = 0;
            recData.dailyHeader.time    = 0;
            recData.dailyHeader.txnType = 0;
        }
        break;
    case DOC_DETAILED_REPORT:
        if (QUERY_IS_USING_DATE_TIME(rquery.filter)) {
            // TODO
            recData.detailedHeader.dateFrom = libAtoi(rquery.startDate);
            recData.detailedHeader.dateTo   = libAtoi(rquery.endDate);
            recData.detailedHeader.timeFrom = libAtoi(rquery.startTime);
            recData.detailedHeader.timeTo   = libAtoi(rquery.endDate);
            recData.detailedHeader.dateNow  = OOP_CALL(sys(), getDate);
            recData.detailedHeader.timeNow  = OOP_CALL(sys(), getTime);
            recData.detailedHeader.txnType  = 0;
        }
        break;
    default:
        break;
    }
    QueryOperator op;
    txnquery()->init(&op);

    if (QUERY_IS_USING_DATE_TIME(rquery.filter)) {
        uint32_t sdate = libAtoi(rquery.startDate);
        uint32_t stime = libAtoi(rquery.startTime);
        uint32_t edate = libAtoi(rquery.endDate);
        uint32_t etime = libAtoi(rquery.endTime);
        uint64_t sdt   = packDateTime(sdate, stime);
        uint64_t ldt   = packDateTime(edate, etime);
        txnquery()->where(&op, TXN_REC_FIELD_TIMESTAMP, SELECT_GTE, &sdt);
        txnquery()->where(&op, TXN_REC_FIELD_TIMESTAMP, SELECT_LTE, &ldt);
    }
    if (QUERY_IS_USING_REF_NUM(rquery.filter)) {
        txnquery()->where(&op, TXN_REC_FIELD_CORE_REF_NUM, SELECT_EQ,
                          &rquery.refNum);
    }
    if (QUERY_IS_USING_SERVICE_ID(rquery.filter)) {
        txnquery()->where(&op, TXN_REC_FIELD_CORE_ID, SELECT_EQ,
                          &rquery.serviceId);
    }
    if (QUERY_IS_USING_TRACE(rquery.filter)) {
        txnquery()->where(&op, TXN_REC_FIELD_CORE_TRACE, SELECT_EQ,
                          &rquery.trace);
    }

    txnrecord()->select(&op, handleExtractedData, (void*)&recData);
}

STATE_DEF_EXIT(ExtractData) {}

STATE_DEF_HANDLE(ExtractData, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        GOTO_INFO(prev, prev, INFO_ERROR, phraseGetDef(PHRASE_NO_RESULT), "");
    }
}

static void ExtractData(State* parent) {
    extractData = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, extractData, parent, "extract data");
    extractData->vtable.enter        = STATE_ENTER(ExtractData);
    extractData->vtable.exit         = STATE_EXIT(ExtractData);
    extractData->vtable.handleKeypad = STATE_HANDLE(ExtractData, KeypadEvent);
}

/******************** Settings sub state **********************/
static Menu* reportsMenu;

static void setReportItem(void* arg) {
    rItem = (PrintItem_t)(uintptr_t)arg;
    prev  = subReports[rItem];
}

STATE_DEF_ENTER(Reports) {
    QUERY_FILTER_RESET(rquery.filter);
    ui_menu_create(reportsMenu, disp()->screen);
    for (uint8_t i = 0; i < REP_ITEM_ALL; i++) {
        ui_menu_addItem(reportsMenu, phraseGetDef(reportsItemTxt[i]),
                        subReports[i], setReportItem, (void*)(uintptr_t)i);
    }
    GOTO_MENU(state->parent, reportsMenu, NULL, NULL);
}

OOP_CTOR(Reports, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    mainMenu                = self;
    self->base.vtable.enter = STATE_ENTER(Reports);

    RePrint(self);
    DailyReport(self);
    SummaryReport(self);
    DetailsReport(self);
    ExtractData(self);

    getStartDate = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, getStartDate, parent, "get Start Date");
    getStartDate->vtable.enter = STATE_ENTER(GetStartDate);

    getStartTime = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, getStartTime, parent, "get Start Time");
    getStartTime->vtable.enter = STATE_ENTER(GetStartTime);

    getEndDate = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, getEndDate, parent, "get End Date");
    getEndDate->vtable.enter = STATE_ENTER(GetEndDate);

    getEndTime = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, getEndTime, parent, "get End Time");
    getEndTime->vtable.enter = STATE_ENTER(GetEndTime);

    printMenu   = MEM_ALLOC(sizeof(*printMenu));
    reportsMenu = MEM_ALLOC(sizeof(*reportsMenu));
}