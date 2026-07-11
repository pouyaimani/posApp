#ifndef RECEIPT_TEMPLATE_H_
#define RECEIPT_TEMPLATE_H_

#include <stdint.h>
#include "txn.h"
#include "receipt.h"

typedef enum {
    DOC_TXN,
    DOC_DAILY_REPORT,
    DOC_SUMMARY_REPORT,
    DOC_DETAILED_REPORT,
} ReceiptDocType;

typedef struct {
    TxnType  txnType;
    uint32_t date;
    uint32_t time;
} DailyReportHeader;

typedef struct {
    TxnType  txnType;
    uint32_t dateFrom;
    uint32_t timeFrom;
    uint32_t dateTo;
    uint32_t timeTo;
    uint32_t dateNow;
    uint32_t timeNow;
} DetailedReportHeader;

typedef struct {
    TxnType  txnType;
    uint32_t dateFrom;
    uint32_t timeFrom;
    uint32_t dateTo;
    uint32_t timeTo;
    uint32_t dateNow;
    uint32_t timeNow;
} SummaryReportHeader;

typedef struct {
    TxnType  txnType;
    uint32_t count;
    uint64_t amntSum;
} SummaryReportBody;

typedef struct {
    ReceiptDocType type;
    TxnData*       txn;
    bool           headerApplied;
    union {
        DailyReportHeader    dailyHeader;
        DetailedReportHeader detailedHeader;
        SummaryReportHeader  summaryHeader;
        SummaryReportBody    summaryBody;
    };
} ReceiptData;

typedef int8_t (*ReceiptBuilder)(Receipt*, const ReceiptData*);

Result_t buildReceipt(Receipt* rec, const ReceiptData* data);

#endif