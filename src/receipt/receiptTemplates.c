#include "receiptTemplates.h"
#include "common.h"
#include "settings/settings.h"
#include "utility/utility.h"
#include "sys/sys.h"

static int8_t receiptSectionPsp(Receipt *rec) {
    // TODO: left side value?
    RecColumn_t row[] = {
        {"910990057", LV_TEXT_ALIGN_LEFT, 1},
        {"پرداخت سبز", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionTerminalInfo(Receipt *rec) {
    DEFINE_STRING(terminal, 64);
    snprintf(terminal, sizeof(terminal), "%s:%s", settings()->terminal.terminalNo, "پایانه");
    DEFINE_STRING(code, 64);
    snprintf(code, sizeof(code), "%s:%s", settings()->terminal.terminalNo, "کد کارتخوان");
    RecColumn_t row[] = {
        {terminal, LV_TEXT_ALIGN_LEFT, 1},
        {code, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionBankName(Receipt *rec, const char *bin) {
        // TODO: bank name
    RecColumn_t row[] = {
        {"bank name", LV_TEXT_ALIGN_LEFT, 1},
        {"بانک", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionRefTrace(Receipt *rec, const uint32_t *trace, const uint32_t *refNum) {
    DEFINE_STRING(traceStr, 64);
    snprintf(traceStr, sizeof(traceStr), "%s:%d", *trace, "پیگیری");
    DEFINE_STRING(refStr, 64);
    snprintf(refStr, sizeof(refStr), "%s:%d", *refNum, "مرجع");
    RecColumn_t row[] = {
        {refStr, LV_TEXT_ALIGN_LEFT, 1},
        {traceStr, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionBalance(Receipt *rec, const uint64_t *balance, const uint64_t *ledger) {
    DEFINE_STRING(balanceStr, 64);
    snprintf(balanceStr, sizeof(balanceStr), "%s %d", *balance, "ریال");
    RecColumn_t row[] = {
        {balanceStr, LV_TEXT_ALIGN_LEFT, 1},
        {"مانده واقعی", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);

    DEFINE_STRING(ledgerStr, 64);
    snprintf(ledgerStr, sizeof(ledgerStr), "%s %d", *ledger, "ریال");
    RecColumn_t row1[] = {
        {ledgerStr, LV_TEXT_ALIGN_LEFT, 1},
        {"مانده قابل برداشت", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionTxnHeader(Receipt *rec, const uint64_t *dateTime) {
    uint32_t date, time;
    unpackDateTime(dateTime, &date, &time);
    DEFINE_STRING(dtStr, 24);
    dateTimeToStr(date, time, dtStr, sizeof(dtStr));
    DEFINE_STRING(header, 64);
    // TODO" service name
    snprintf(header, sizeof(header), "%s-%s-%s", dtStr, "رسید مشتری", "نام سرویس");
    return ERR_OK;
}

static int8_t receiptSectionAmount(Receipt *rec, const uint64_t *amount) {
    DEFINE_STRING(amountStr, 24);
    snprintf(amountStr, sizeof(amountStr), "%s %d", *amount, "ریال");
    RecColumn_t row[] = {
        {amountStr, LV_TEXT_ALIGN_LEFT, 1},
        {"عملیات موفق", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionFailure(Receipt *rec, uint16_t respCode) {
    DEFINE_STRING(desc, 24);
    // TODO: error description
    snprintf(desc, sizeof(desc), "%s (%d)", desc, "error description", respCode);
    RecColumn_t row[] = {
        {desc, LV_TEXT_ALIGN_CENTER, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionOperatorPhone(Receipt *rec, const char *phone, SimCardOperators_t operaor) {
    // TODO: operator name
    RecColumn_t row[] = {
        {"operator", LV_TEXT_ALIGN_LEFT, 1},
        {phone, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionChargeCode(Receipt *rec, const char *serial, const char *pin, SimCardOperators_t operaor) {
    // TODO: operator name
    RecColumn_t row[] = {
        {serial, LV_TEXT_ALIGN_LEFT, 1},
        {"سریال شارژ", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    RecColumn_t row1[] = {
        {pin, LV_TEXT_ALIGN_LEFT, 1},
        {"رمز شارژ", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    DEFINE_STRING(command, 16);
    snprintf(command, sizeof(command), "%s %s", "", "*رمز شارژ#");
    // TODO: sim operator
    RecColumn_t row2[] = {
        {command, LV_TEXT_ALIGN_LEFT, 1},
        {"رمز شارژ", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t buildSaleReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData *txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTxnHeader(rec, &txn->dateTime), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, &txn->core.trace,
                            &txn->core.refNum), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildBillReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildTopupReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData *txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTxnHeader(rec, &txn->dateTime), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, &txn->core.trace, 
                                    &txn->core.refNum), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionOperatorPhone(rec, "phone num",
                                    SIM_OP_IRANCELL), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildBalanceReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData *txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTxnHeader(rec, &txn->dateTime), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, &txn->core.trace, 
                            &txn->core.refNum), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBalance(rec, 
                            &txn->extention.balance.balance,
                                &txn->extention.balance.ledger), 
                                        ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildPaymentReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);    
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildChargeCodeReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData *txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTxnHeader(rec, &txn->dateTime), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, &txn->core.trace,
                                &txn->core.refNum), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, 
                                &txn->core.amount), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildDailyRepHeaderReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    DailyReportHeader *header = &data->dailyHeader;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ; , ERR_NOK);
    RecColumn_t row[] = {
        {"گزارش روزانه همه تراکنش ها", LV_TEXT_ALIGN_CENTER, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ; , ERR_NOK);
    // Date Time
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildDailyRepBodyReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData *txn = &data->txn;
    uint32_t date, time;
    DEFINE_STRING(dtStr, 24);
    unpackDateTime(txn->dateTime, &date, &time);
    dateTimeToStr(date, time, dtStr, sizeof(dtStr));
    // TODO" service name
    DEFINE_STRING(title, 32);
    snprintf(title, sizeof(title), "%s-%s", dtStr, "نام سرویس");
    RecColumn_t row[] = {
        {title, LV_TEXT_ALIGN_CENTER, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, "bin"), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, &txn->core.trace, 
                            &txn->core.refNum), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildAggRepHeaderReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);    
    AggregateReportHeader *header = &data->aggregateHeader;
    RecColumn_t row[] = {
        {"گزارش تجمیعی", LV_TEXT_ALIGN_CENTER, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ; , ERR_NOK);
    DATE_TIME_STR(dt);
    dateTimeToStr(header->dateNow, header->timeNow, dt, sizeof(dt));
    DEFINE_STRING(day, 24);
    uint32_t date = OOP_CALL(sys(), getDate);
    getNameofDay(date, day, sizeof(day));
    RecColumn_t row1[] = {
        {day, LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row1), ERR_OK, ; , ERR_NOK);
    dateTimeToStr(header->dateFrom, header->timeFrom, dt, sizeof(dt));
    RecColumn_t row2[] = {
        {"از تاریخ", LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row2), ERR_OK, ; , ERR_NOK);
    dateTimeToStr(header->dateTo, header->timeTo, dt, sizeof(dt));
    RecColumn_t row3[] = {
        {"تا تاریخ", LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row3), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ; , ERR_NOK);
    RecColumn_t row4[] = {
        {"مبغ به ریال", LV_TEXT_ALIGN_LEFT, 1},
        {"تعداد", LV_TEXT_ALIGN_LEFT, 1},
        {"تراکنش", LV_TEXT_ALIGN_LEFT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 3, row4), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildAggRepBodyReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    AggregateReportBody *body = &data->aggregateBody;
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
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 3, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailtRepHeaderReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ; , ERR_NOK);
    DetailedReportHeader *header = &data->detailedHeader;
    RecColumn_t row[] = {
        {"ریزتراکنش همه تراکنش ها", LV_TEXT_ALIGN_CENTER, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ; , ERR_NOK);
    DATE_TIME_STR(dt);
    dateTimeToStr(header->dateNow, header->timeNow, dt, sizeof(dt));
    RecColumn_t row1[] = {
        {"today", LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row1), ERR_OK, ; , ERR_NOK);
    dateTimeToStr(header->dateFrom, header->timeFrom, dt, sizeof(dt));
    RecColumn_t row2[] = {
        {"از تاریخ", LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row2), ERR_OK, ; , ERR_NOK);
    dateTimeToStr(header->dateTo, header->timeTo, dt, sizeof(dt));
    RecColumn_t row3[] = {
        {"تا تاریخ", LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row3), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ; , ERR_NOK);
    RecColumn_t row4[] = {
        {"تاریخ", LV_TEXT_ALIGN_LEFT, 1},
        {"پیگیری", LV_TEXT_ALIGN_LEFT, 1},
        {"مبغ به ریال", LV_TEXT_ALIGN_LEFT, 1},
        {"تراکنش", LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 4, row4), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailRepBodyReceipt(Receipt *rec, const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData *txn = &data->txn;
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
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTable, 4, row4), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static const ReceiptTemplate templates[] = {
    { REC_SALE,                                 buildSaleReceipt                },
    { REC_BILL,                                 buildBillReceipt                },
    { REC_TOPUP,                                buildTopupReceipt               },
    { REC_BALANCE,                              buildBalanceReceipt             },
    { REC_PAY,                                  buildPaymentReceipt             },
    { REC_SIM_CHARGE_CODE,                      buildChargeCodeReceipt          },
    { REC_DAILY_REPORT_HEADER,                  buildDailyRepHeaderReceipt      },
    { REC_DAILY_REPORT_BODY,                    buildDailyRepBodyReceipt        },
    { REC_AGGREGATION_REPORT_HEADER,            buildAggRepHeaderReceipt        },
    { REC_AGGREGATION_REPORT_BODY,              buildAggRepBodyReceipt          },
    { REC_DETAIL_REPORT_HEADER,                 buildDetailtRepHeaderReceipt    },
    { REC_DETAIL_REPORT_BODY,                   buildDetailRepBodyReceipt       }
};

int8_t buildReceipt(Receipt *rec,
                        TxnType type,
                            const ReceiptData *data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(createReceipt(&rec), true, 
    RECEIPT_CREATE_ERROR(), ERR_MEMORY_ALLOCATION);
    size_t i;

    for (i = 0; i < ARRAY_SIZE(templates); i++) {

        if (templates[i].type == type) {
            return templates[i].builder(rec, data);
        }
    }

    return ERR_NOT_SUPPORTED;
}