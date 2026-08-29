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
#include "record/txnQuery.h"

static ReceiptDocType docType;

static TxnQueryDsc rquery;

static TxnQueryFilter qfilter;

#define REPORT_RESULT_TXN(x) (x = REP_RESULT_TXN)

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

static const Phrases_t reportsItemTxt[REP_ITEM_ALL] = {
    PHRASE_REPRINT, PHRASE_DAILY_REPORT, PHRASE_SUMMARY_REPORT,
    PHRASE_TRANACTION_DETAILS};

/*********************************************************************************************
 *                                                                                           *
 *                                      Helpers
 *                                                                                           *
 ********************************************************************************************/

static void startReprintByTxnType(TxnType txnType) {
    rquery.txnType = txnType;
    queryFilterSet(&qfilter, REP_FILTER_TXN_TYPE);
    SM_GOTO(extractData);
}

static void runReportInput(InputMode_t mode, const char* title, char* output,
                           size_t outputSize, uint16_t max, SubState* next) {
    inmgr()->run(&(InputCfg){.type   = INPUT_TYPE_KEYPAD,
                             .mode   = mode,
                             .title  = title,
                             .info   = "",
                             .maxLen = max},
                 mainMenu, next);

    inmgr()->setOut(output, NULL, outputSize);
}

/*********************************************************************************************
 *                                                                                           *
 *                                Re print sub state
 *                                                                                           *
 ********************************************************************************************/
typedef enum {
    REPRINT_ALL = 0,
    REPRINT_PURCHASE,
    REPRINT_BILL,
    REPRINT_VOUCHER,
    REPRINT_TOPUP,
    REPRINT_TRACE,
    REPRINT_REF,
    REPRINT_END
} PrintItem_t;

static PrintItem_t pItem;

void setReprintItem(void* arg) {
    pItem = (PrintItem_t)(*((uint16_t*)arg));
    LOG_TRACE("Print Item = %d", pItem);
    switch (pItem) {
    case REPRINT_TRACE:
        queryFilterSet(&qfilter, REP_FILTER_TRACE);
        runReportInput(INMD_ENTER_NUMBERS, phraseGetDef(PHRASE_ENTER_TRACE),
                       rquery.trace, sizeof(rquery.trace), LEN_MAX_TRACE_IN,
                       extractData);
        break;
    case REPRINT_REF:
        queryFilterSet(&qfilter, REP_FILTER_REF_NUM);
        runReportInput(INMD_ENTER_NUMBERS, phraseGetDef(PHRASE_ENTER_REF_NUM),
                       rquery.refNum, sizeof(rquery.refNum), LEN_MAX_REF_NUM_IN,
                       extractData);
        break;
    case REPRINT_BILL:
        startReprintByTxnType(TXN_BILL);
        break;
    case REPRINT_VOUCHER:
        startReprintByTxnType(TXN_VOUCHER);
        break;
    case REPRINT_PURCHASE:
        startReprintByTxnType(TXN_PURCHASE);
        break;
    case REPRINT_TOPUP:
        startReprintByTxnType(TXN_TOPUP);
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
    PHRASE_TXN_ALL,         PHRASE_TXN_PURCHASE, PHRASE_TXN_BILL,
    PHRASE_TXN_TOPUP,       PHRASE_TXN_VOUCHER,  PHRASE_BASED_ON_TRACE,
    PHRASE_BASED_ON_REF_NUM};

static Menu* printMenu;

STATE_DEF_ENTER(RePrint) {
    docType = DOC_TXN;
    queryFilterSet(&qfilter, REP_FILTER_LATEST);
    REPORT_RESULT_TXN(rquery.resMode);
    ui_menu_create(printMenu, disp()->screen);
    for (uint8_t i = 0; i < REPRINT_END; i++) {
        ui_menu_addItem(printMenu, phraseGetDef(printItemTxt[i]),
                        LV_TEXT_ALIGN_RIGHT, NULL, setReprintItem, NULL);
    }
    GOTO_MENU(state->parent, printMenu, NULL, NULL);
}

static void RePrint(State* parent) {
    subReports[REP_ITEM_REPRINT] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_REPRINT], parent, "reprint");
    subReports[REP_ITEM_REPRINT]->vtable.enter = STATE_ENTER(RePrint);
}

/*********************************************************************************************
 *                                                                                           *
 *                                Daily reports sub state
 *                                                                                           *
 ********************************************************************************************/

STATE_DEF_ENTER(DailyReport) {
    docType = DOC_DAILY_REPORT;
    SM_GOTO(extractData);
}

static void DailyReport(State* parent) {
    subReports[REP_ITEM_DAILY] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReports[REP_ITEM_DAILY], parent, "daily reports");
    subReports[REP_ITEM_DAILY]->vtable.enter = STATE_ENTER(DailyReport);
}

/*********************************************************************************************
 *                                                                                           *
 *                                Summary report sub state
 *                                                                                           *
 ********************************************************************************************/

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

/*********************************************************************************************
 *                                                                                           *
 *                                Detail report sub state
 *                                                                                           *
 ********************************************************************************************/

STATE_DEF_ENTER(DetailsReport) {
    docType = DOC_DETAILED_REPORT;
    ui_menu_create(printMenu, disp()->screen);
    for (uint8_t i = 0; i < REPRINT_TRACE; i++) {
        ui_menu_addItem(printMenu, printItemTxt[i], LV_TEXT_ALIGN_RIGHT, NULL,
                        setReprintItem, (void*)(uintptr_t)i);
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

/*********************************************************************************************
 *                                                                                           *
 *                                Get Start Date sub state
 *                                                                                           *
 ********************************************************************************************/

STATE_DEF_ENTER(GetStartDate) {
    runReportInput(INMD_ENTER_DATE, phraseGetDef(PHRASE_FROM_DATE),
                   rquery.startDate, sizeof(rquery.startDate), LEN_MAX_DATE_IN,
                   getStartTime);
}

/*********************************************************************************************
 *                                                                                           *
 *                               Get End Date sub state
 *                                                                                           *
 ********************************************************************************************/

STATE_DEF_ENTER(GetEndDate) {
    runReportInput(INMD_ENTER_DATE, phraseGetDef(PHRASE_TO_DATE),
                   rquery.endDate, sizeof(rquery.endDate), LEN_MAX_DATE_IN,
                   getEndTime);
}

/*********************************************************************************************
 *                                                                                           *
 *                               Get Start Time sub state
 *                                                                                           *
 ********************************************************************************************/

STATE_DEF_ENTER(GetStartTime) {
    runReportInput(INMD_ENTER_TIME, phraseGetDef(PHRASE_FROM_TIME),
                   rquery.startTime, sizeof(rquery.startTime), LEN_MAX_TIME_IN,
                   getEndDate);
}

/*********************************************************************************************
 *                                                                                           *
 *                               Get End Time sub state
 *                                                                                           *
 ********************************************************************************************/

STATE_DEF_ENTER(GetEndTime) {
    runReportInput(INMD_ENTER_TIME, phraseGetDef(PHRASE_TO_TIME),
                   rquery.endTime, sizeof(rquery.endTime), LEN_MAX_TIME_IN,
                   extractData);
}

/*********************************************************************************************
 *                                                                                           *
 *                               extract data sub state
 *                                                                                           *
 ********************************************************************************************/

typedef ErrorDsc_t (*ExtractHandlerFn)(ReceiptData* rec, QueryOperator* op);

typedef struct {
    ReceiptDocType   doc;
    ExtractHandlerFn handler;
} ExtractHandler;

static uint64_t            rrn;
static uint32_t            trace;
static uint32_t            fdate;
static uint32_t            ftime;
static uint32_t            tdate;
static uint32_t            ttime;
static QueryDateTimeFilter dtFilter;

static bool handleTxnExtractedData(const TxnData* txn, void* userData) {
    logTxnCore(txn);
    ReceiptData* data = (ReceiptData*)userData;
    memcpy(&data->txn, txn, sizeof(*txn));
    Receipt  rec;
    Result_t res = buildReceipt(&rec, data);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    res = OOP_CALL(&rec, flush);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    OOP_CALL(&rec, destroy);
    return true;
}

static bool handleSummaryExtractedData(TxnType               txnType,
                                       const AggDataSummary* aggDataSum,
                                       void*                 userData) {
    ReceiptData* data = (ReceiptData*)userData;
    Receipt      rec;
    Result_t     res = buildReceipt(&rec, data);
    for (size_t i = 0; i < aggDataSum->txnCount; i++) {
        LOG_TRACE("aggregate: txn type = %d, txn count = %llu, txn sum = %llu",
                  aggDataSum->data[i].txn, aggDataSum->data[i].count,
                  aggDataSum->data[i].sum);
        data->summaryBody.txnType = aggDataSum->data[i].txn;
        data->summaryBody.count   = aggDataSum->data[i].count;
        data->summaryBody.amntSum = aggDataSum->data[i].sum;
        buildReceipt(&rec, data);
    }
    res = OOP_CALL(&rec, flush);
    // RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    OOP_CALL(&rec, destroy);
    return true;
}

static bool handleDailyExtractedData(TxnType txnType, uint32_t count,
                                     const uint64_t* sums, uint32_t sumCount,
                                     void* userData) {
    LOG_DEBUG("Summary report: txn type = %d, number of txns = %lu, total "
              "amount = %llu",
              txnType, count, sums[0]);
}

static bool handleDetailedExtractedData(TxnType txnType, uint32_t count,
                                        const uint64_t* sums, uint32_t sumCount,
                                        void* userData) {
    LOG_DEBUG("Summary report: txn type = %d, number of txns = %lu, total "
              "amount = %llu",
              txnType, count, sums[0]);
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

static ErrorDsc_t handleTxnExtracting(ReceiptData* rec, QueryOperator* op) {
    Result_t res = txnrecord()->select(op, handleTxnExtractedData, (void*)rec);
    return res.err;
}

static ErrorDsc_t handleSummaryExtracting(ReceiptData* rec, QueryOperator* op) {
    rec->summaryHeader.dateFrom = fdate;
    rec->summaryHeader.dateTo   = tdate;
    rec->summaryHeader.timeFrom = ftime;
    rec->summaryHeader.timeTo   = ttime;
    rec->summaryHeader.dateNow  = OOP_CALL(sys(), getDate);
    rec->summaryHeader.timeNow  = OOP_CALL(sys(), getTime);
    Result_t res                = txnrecord()->aggregate(
        op, TXN_ALL, handleSummaryExtractedData, (void*)rec);
    return res.err;
}

static ErrorDsc_t handleDailyExtracting(ReceiptData* rec, QueryOperator* op) {
    if (queryFilterIsSet(qfilter, REP_FILTER_DATE_TIME)) {
        // TODO
        rec->dailyHeader.date    = 0;
        rec->dailyHeader.time    = 0;
        rec->dailyHeader.txnType = 0;
    }
    return ERR_DSC_OK;
}

static ErrorDsc_t handleDetailExtracting(ReceiptData* rec, QueryOperator* op) {
    return ERR_DSC_OK;
}

static const ExtractHandler exTemplates[] = {
    /******************************************************************/
    /*Txn Reports*/
    /******************************************************************/
    {.doc = DOC_TXN, .handler = handleTxnExtracting},
    /******************************************************************/
    /*Summary Reports*/
    /******************************************************************/
    {.doc = DOC_SUMMARY_REPORT, .handler = handleSummaryExtracting},
    /******************************************************************/
    /*Daily Reports*/
    /******************************************************************/
    {.doc = DOC_DAILY_REPORT, .handler = handleDailyExtracting},
    /******************************************************************/
    /*Detialed Reports*/
    /******************************************************************/
    {.doc = DOC_DETAILED_REPORT, .handler = handleDetailExtracting},
};

const ExtractHandler* findExtracter(ReceiptDocType doc) {
    for (size_t i = 0; i < ARRAY_SIZE(exTemplates); i++) {
        if (exTemplates[i].doc == doc) {
            return &exTemplates[i];
        }
    }
    return NULL;
}

STATE_DEF_ENTER(ExtractData) {
    ReceiptData recData;
    recData.headerApplied = false;
    recData.type          = docType;
    LOG_TRACE("Extract report data: doc type = %d", docType);
    LOG_DEBUG("start date = %s, start time = %s, end date = %s, end time = %s",
              rquery.startDate, rquery.startTime, rquery.endDate,
              rquery.endTime);

    QueryOperator op;
    Result_t      res = txnquery()->init(&op);
    if (res.err != ERR_DSC_OK) {
        LOG_ERROR("query initialization failed.");
        txnquery()->close(&op);
        SM_GOTO(mainMenu);
        return;
    }

    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_EXTRACTING_DATA),
              phraseGetDef(PHRASE_PLEASE_WAIT));
    OOP_CALL(infoPage(), forceUpdate);

    STRING_TO_U64(rquery.refNum, &rrn);
    STRING_TO_U32(rquery.trace, &trace);
    STRING_TO_U64(rquery.refNum, &rrn);

    STRING_TO_U32(rquery.startDate, &fdate);
    STRING_TO_U32(rquery.startTime, &ftime);
    STRING_TO_U32(rquery.endDate, &tdate);
    STRING_TO_U32(rquery.endTime, &ttime);

    dtFilter.fdt = packDateTime(fdate, ftime);
    dtFilter.tdt = packDateTime(tdate, ttime);

    applyFilter(REP_FILTER_DATE_TIME, qfilter, &op, &dtFilter);
    applyFilter(REP_FILTER_REF_NUM, qfilter, &op, &rrn);
    applyFilter(REP_FILTER_TRACE, qfilter, &op, &trace);
    applyFilter(REP_FILTER_TXN_TYPE, qfilter, &op, &rquery.txnType);
    applyFilter(REP_FILTER_LATEST, qfilter, &op, NULL);

    ExtractHandler* extracter = findExtracter(docType);
    if (!extracter) {
        LOG_TRACE("extracter not found");
        GOTO_INFO(mainMenu, mainMenu, INFO_ERROR,
                  phraseGetDef(PHRASE_TXN_NOT_FND), "");
        return;
    }
    res.err = extracter->handler(&recData, &op);
    txnquery()->close(&op);
    OOP_CALL(infoPage(), hide);
    if (res.err != ERR_DSC_OK) {
        GOTO_INFO(mainMenu, mainMenu, INFO_ERROR,
                  phraseGetDef(PHRASE_TXN_NOT_FND), "");
        return;
    }
    SM_GOTO(mainMenu);
}

STATE_DEF_HANDLE(ExtractData, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        GOTO_INFO(prev, prev, INFO_ERROR, phraseGetDef(PHRASE_NO_RESULT), "");
    }
}

static void ExtractData(State* parent) {
    extractData = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, extractData, parent, "extract data");
    extractData->vtable.enter        = STATE_ENTER(ExtractData);
    extractData->vtable.handleKeypad = STATE_HANDLE(ExtractData, KeypadEvent);
}

/*********************************************************************************************
 *                                                                                           *
 *                               Settings sub state
 *                                                                                           *
 ********************************************************************************************/
static Menu* reportsMenu;

static void setReportItem(void* arg) {
    rItem = (PrintItem_t)(uintptr_t)arg;
    prev  = subReports[rItem];
}

STATE_DEF_ENTER(Reports) {
    queryFilterClear(&qfilter);
    ui_menu_create(reportsMenu, disp()->screen);
    for (uint8_t i = 0; i < REP_ITEM_ALL; i++) {
        ui_menu_addItem(reportsMenu, phraseGetDef(reportsItemTxt[i]),
                        LV_TEXT_ALIGN_RIGHT, subReports[i], setReportItem,
                        (void*)(uintptr_t)i);
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