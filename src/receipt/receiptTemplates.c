#include "receiptTemplates.h"
#include "common.h"
#include "settings/settings.h"
#include "utility/utility.h"
#include "sys/sys.h"
#include "phrases/phrases.h"
#include "logger.h"

static int8_t receiptSectionPsp(Receipt* rec) {
    // TODO: left side value?
    RecColumn_t row[] = {
        {"910990057", LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_PSP_GREEN_PAYMENT), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionTerminalInfo(Receipt* rec) {
    DEFINE_STRING(terminal, 64);
    snprintf(terminal, sizeof(terminal), "%s:%s",
             settings()->terminal.terminalNo, phraseGetDef(PHRASE_TERMINAL));
    DEFINE_STRING(code, 64);
    snprintf(code, sizeof(code), "%s:%s", settings()->terminal.terminalNo,
             phraseGetDef(PHRASE_DEVICE_CODE));
    RecColumn_t row[] = {{code, LV_TEXT_ALIGN_LEFT, 1},
                         {terminal, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionBankName(Receipt* rec, const char* bin) {
    // TODO: bank name
    RecColumn_t row[] = {{"bank name", LV_TEXT_ALIGN_LEFT, 1},
                         {phraseGetDef(PHRASE_BANK), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionRefTrace(Receipt* rec, const uint32_t* trace,
                                     const uint32_t* refNum) {
    DEFINE_STRING(traceStr, 64);
    snprintf(traceStr, sizeof(traceStr), "%s:%d", phraseGetDef(PHRASE_TRACE),
             *trace);
    DEFINE_STRING(refStr, 64);
    snprintf(refStr, sizeof(refStr), "%s:%d", phraseGetDef(PHRASE_REFERENCE),
             *refNum);
    RecColumn_t row[] = {{refStr, LV_TEXT_ALIGN_LEFT, 1},
                         {traceStr, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionBalance(Receipt* rec, const uint64_t* balance,
                                    const uint64_t* ledger) {
    DEFINE_STRING(balanceStr, 64);
    snprintf(balanceStr, sizeof(balanceStr), "%s %d", phraseGetDef(PHRASE_RIAL),
             *balance);
    RecColumn_t row[] = {{balanceStr, LV_TEXT_ALIGN_LEFT, 1},
                         {phraseGetDef(PHRASE_LEDGER), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);

    DEFINE_STRING(ledgerStr, 64);
    snprintf(ledgerStr, sizeof(ledgerStr), "%s %d", *ledger,
             phraseGetDef(PHRASE_RIAL));
    RecColumn_t row1[] = {
        {ledgerStr, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_AVAILABLE), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionTxnHeader(Receipt* rec, const uint64_t* dateTime,
                                      TxnType type) {
    uint32_t date, time;
    unpackDateTime(dateTime, &date, &time);
    DEFINE_STRING(dtStr, 24);
    dateTimeToStr(date, time, dtStr, sizeof(dtStr));
    DEFINE_STRING(header, 64);
    DEFINE_STRING(servName, 24);
    getTxnName(type, servName, sizeof(servName));
    snprintf(header, sizeof(header), "%s-%s-%s", servName,
             phraseGetDef(PHRASE_CUSTOMER_RECEIPT), dtStr);
    RecColumn_t row[] = {
        {header, LV_TEXT_ALIGN_CENTER, 1},
    };
    return OOP_CALL(rec, addTextWithBorder, 1, row);
}

static int8_t receiptSectionAmount(Receipt* rec, const uint64_t* amount) {
    DEFINE_STRING(amountStr, 36);
    DEFINE_STRING(amountSep, 24);
    snprintf(amountStr, sizeof(amountStr), "%llu", *amount);
    amountSeparator(amountStr, amountSep, sizeof(amountSep));
    memset(amountStr, 0, sizeof(amountStr));
    snprintf(amountStr, sizeof(amountStr), "%s %s", amountStr,
             phraseGetDef(PHRASE_RIAL));
    RecColumn_t row[] = {
        {amountStr, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SUCCESSFUL_OPERATION), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 2, row), ERR_OK, ;
                        , ERR_NOK);
}

static int8_t receiptSectionFailure(Receipt* rec, uint16_t respCode) {
    DEFINE_STRING(desc, 24);
    // TODO: error description
    snprintf(desc, sizeof(desc), "%s (%d)", desc, "error description",
             respCode);
    RecColumn_t row[] = {{desc, LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ;
                        , ERR_NOK);
}

static int8_t receiptSectionOperatorPhone(Receipt* rec, const char* phone,
                                          SimCardOperators_t operaor) {
    // TODO: operator name
    RecColumn_t row[] = {{"operator", LV_TEXT_ALIGN_LEFT, 1},
                         {phone, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
}

static int8_t receiptSectionChargeCode(Receipt* rec, const char* serial,
                                       const char*        pin,
                                       SimCardOperators_t operaor) {
    // TODO: operator name
    RecColumn_t row[] = {
        {serial, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SIM_CHARGE_SERIAL), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
    RecColumn_t row1[] = {
        {pin, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SIM_CHARGE_PIN), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
    DEFINE_STRING(command, 16);
    snprintf(command, sizeof(command), "%s %s", "", "*رمز شارژ#");
    // TODO: sim operator
    RecColumn_t row2[] = {
        {command, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SIM_CHARGE_PIN), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ;, ERR_NOK);
}

static int8_t buildSaleReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.refNum),
        ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    // RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildBillReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildTopupReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.refNum),
        ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionOperatorPhone(rec, "phone num", SIM_OP_IRANCELL), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildBalanceReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.refNum),
        ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBalance(rec,
                                              &txn->extention.balance.balance,
                                              &txn->extention.balance.ledger),
                        ERR_OK,
                        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildPaymentReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildChargeCodeReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.refNum),
        ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDailyRepHeaderReceipt(Receipt*           rec,
                                         const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    DailyReportHeader* header = &data->dailyHeader;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    DEFINE_STRING(txt, 36);
    snprintf(txt, sizeof(txt), "%s %s", phraseGetDef(PHRASE_DAILY_REPORT),
             phraseGetDef(PHRASE_TXN_ALL));
    RecColumn_t row[] = {
        {"گزارش روزانه همه تراکنش ها", LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ;
                        , ERR_NOK);
    // Date Time
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDailyRepBodyReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = data->txn;
    uint32_t date, time;
    DEFINE_STRING(dtStr, 24);
    unpackDateTime(txn->dateTime, &date, &time);
    dateTimeToStr(date, time, dtStr, sizeof(dtStr));
    // TODO" service name
    DEFINE_STRING(title, 32);
    snprintf(title, sizeof(title), "%s-%s", dtStr,
             phraseGetDef(PHRASE_SERVICE_NAME));
    RecColumn_t row[] = {{title, LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.refNum),
        ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    return ERR_OK;
}

static int8_t buildSumRepHeaderReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    SummaryReportHeader* header = &data->summaryHeader;
    RecColumn_t          row[]  = {
        {phraseGetDef(PHRASE_SUMMARY_REPORT), LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ;
                        , ERR_NOK);
    DATE_TIME_STR(dt);
    dateTimeToStr(header->dateNow, header->timeNow, dt, sizeof(dt));
    DEFINE_STRING(day, 24);
    uint32_t date = OOP_CALL(sys(), getDate);
    getNameofDay(date, day, sizeof(day));
    RecColumn_t row1[] = {
        {day, LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row1), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateFrom, header->timeFrom, dt, sizeof(dt));
    RecColumn_t row2[] = {
        {phraseGetDef(PHRASE_FROM_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row2), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateTo, header->timeTo, dt, sizeof(dt));
    RecColumn_t row3[] = {
        {phraseGetDef(PHRASE_TO_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row3), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RecColumn_t row4[] = {{phraseGetDef(PHRASE_TO_RIAL), LV_TEXT_ALIGN_LEFT, 1},
                          {phraseGetDef(PHRASE_COUNT), LV_TEXT_ALIGN_LEFT, 1},
                          {phraseGetDef(PHRASE_TXN), LV_TEXT_ALIGN_LEFT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 3, row4), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildSumRepBodyReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    SummaryReportBody* body = &data->summaryBody;
    DEFINE_STRING(countStr, 16);
    snprintf(countStr, sizeof(countStr), "%u", body->count);
    AMOUNT_STR(amntStr);
    snprintf(amntStr, sizeof(amntStr), "%llu", body->amntSum);
    DEFINE_STRING(amntStrSep, 24);
    amountSeparator(amntStr, amntStrSep, sizeof(amntStrSep));
    DEFINE_STRING(txnName, 64);
    getTxnName(body->txnType, txnName, sizeof(txnName));
    RecColumn_t row[] = {
        {amntStr, LV_TEXT_ALIGN_LEFT, 1},
        {countStr, LV_TEXT_ALIGN_LEFT, 1},
        {txnName, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 3, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailtRepHeaderReceipt(Receipt*           rec,
                                           const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    DetailedReportHeader* header = &data->detailedHeader;
    DEFINE_STRING(txt, 36);
    snprintf(txt, sizeof(txt), "%s %s", phraseGetDef(PHRASE_DETAILED_REPORT),
             phraseGetDef(PHRASE_TXN_ALL));
    RecColumn_t row[] = {{txt, LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ;
                        , ERR_NOK);
    DATE_TIME_STR(dt);
    dateTimeToStr(header->dateNow, header->timeNow, dt, sizeof(dt));
    uint32_t date = OOP_CALL(sys(), getDate);
    DEFINE_STRING(today, 24);
    getNameofDay(date, today, sizeof(today));
    RecColumn_t row1[] = {
        {today, LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row1), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateFrom, header->timeFrom, dt, sizeof(dt));
    RecColumn_t row2[] = {
        {phraseGetDef(PHRASE_FROM_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row2), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateTo, header->timeTo, dt, sizeof(dt));
    RecColumn_t row3[] = {
        {phraseGetDef(PHRASE_TO_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row3), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RecColumn_t row4[] = {
        {phraseGetDef(PHRASE_DATE), LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_TRACE), LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_TO_RIAL), LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_TXN), LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 4, row4), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailRepBodyReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = data->txn;
    DATE_TIME_STR(dt);
    uint16_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    shortDateTimeToStr(date, time, dt, sizeof(dt));
    DEFINE_STRING(traceStr, 16);
    snprintf(traceStr, sizeof(traceStr), "%u", txn->core.trace);
    AMOUNT_STR(amntStr);
    snprintf(amntStr, sizeof(amntStr), "%llu", txn->core.amount);
    DEFINE_STRING(amntStrSep, 24);
    amountSeparator(amntStr, amntStrSep, sizeof(amntStrSep));
    DEFINE_STRING(txnName, 64);
    getTxnName(txn->core.txnType, txnName, sizeof(txnName));
    RecColumn_t row4[] = {
        {dt, LV_TEXT_ALIGN_LEFT, 1},
        {traceStr, LV_TEXT_ALIGN_LEFT, 1},
        {amntStr, LV_TEXT_ALIGN_LEFT, 1},
        {txnName, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 4, row4), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailRepReceipt(Receipt* rec, const ReceiptData* data) {}

static int8_t buildDailyRepReceipt(Receipt* rec, const ReceiptData* data) {}

static int8_t buildSumRepReceipt(Receipt* rec, const ReceiptData* data) {}

int8_t buildReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(createReceipt(rec), ERR_OK, RECEIPT_CREATE_ERROR(),
                        ERR_MEMORY_ALLOCATION);
    ReceiptBuilder builder = NULL;
    if (data->type == DOC_TXN) {
        switch (data->txn->core.txnType) {
        case TXN_SALE:
            builder = buildSaleReceipt;
            break;
        case TXN_BILL:
            builder = buildBillReceipt;
            break;
        case TXN_TOPUP:
            builder = buildTopupReceipt;
            break;
        case TXN_BALANCE:
            builder = buildBalanceReceipt;
            break;
        case TXN_PAY:
            builder = buildPaymentReceipt;
            break;
        case TXN_SIM_CHARGE:
            builder = buildChargeCodeReceipt;
            break;
        default:
            break;
        }
    } else {
        switch (data->type) {
        case DOC_DAILY_REPORT:
            builder = buildDailyRepReceipt;
            break;
        case DOC_SUMMARY_REPORT:
            builder = buildSumRepReceipt;
            break;
        case DOC_DETAILED_REPORT:
            builder = buildDetailRepReceipt;
            break;
        default:
            break;
        }
    }

    RETURN_VALUE_IF_NULL(builder, ;, ERR_NOT_SUPPORTED);
    return builder(rec, data);
}