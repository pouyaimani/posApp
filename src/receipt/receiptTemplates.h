#ifndef RECEIPT_TEMPLATE_H_
#define RECEIPT_TEMPLATE_H_

#include <stdint.h>
#include "txn.h"
#include "receipt.h"

typedef enum {
    REC_SALE = 0,
	REC_BILL,
	REC_TOPUP,
	REC_BALANCE,
	REC_PAY,
	REC_SIM_CHARGE_CODE,
    REC_DAILY_REPORT_HEADER,
    REC_DAILY_REPORT_BODY,
    REC_AGGREGATION_REPORT_HEADER,
    REC_AGGREGATION_REPORT_BODY,
    REC_DETAIL_REPORT_HEADER,
    REC_DETAIL_REPORT_BODY
} ReceiptType_t;

typedef enum {
    DOC_TXN,
    DOC_DAILY_REPORT,
    DOC_AGGREGATION_REPORT,
    DOC_DETAILED_REPORT,
    DOC_SETTLEMENT_REPORT,
} ReceiptDocType;

typedef struct {
    TxnType txnType;
    uint32_t date;
    uint32_t time;
} DailyReportHeader;

typedef struct {
    TxnType txnType;
    uint32_t dateFrom;
    uint32_t timeFrom;
    uint32_t dateTo;
    uint32_t timeTo;
    uint32_t dateNow;
    uint32_t timeNow;
} DetailedReportHeader;

typedef struct {
    TxnType txnType;
    uint32_t dateFrom;
    uint32_t timeFrom;
    uint32_t dateTo;
    uint32_t timeTo;
    uint32_t dateNow;
    uint32_t timeNow;
} AggregateReportHeader;

typedef struct {
    TxnType txnType;
    uint32_t count;
    uint64_t amntSum;
} AggregateReportBody;

typedef struct {
    ReceiptDocType type;

    union {
        TxnData txn;
        DailyReportHeader dailyHeader;
        DetailedReportHeader detailedHeader;
        AggregateReportHeader aggregateHeader;
        AggregateReportBody aggregateBody;
    };
} ReceiptData;

typedef int8_t (*ReceiptBuilder)(
    Receipt *,
    const ReceiptData *);

typedef struct {
    TxnType type;
    ReceiptBuilder builder;
} ReceiptTemplate;

int8_t buildReceipt(
    Receipt *rec,
    TxnType type,
    const ReceiptData *data);

#endif