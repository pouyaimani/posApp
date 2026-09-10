#ifndef TXNQUERY_H_
#define TXNQUERY_H_

#include "common.h"
#include "txn.h"
#include "txnRecs.h"

typedef enum {
    REP_FILTER_DATE_TIME = (1u << 0),
    REP_FILTER_REF_NUM   = (1u << 1),
    REP_FILTER_TRACE     = (1u << 2),
    REP_FILTER_TXN_TYPE  = (1u << 3),
    REP_FILTER_LATEST    = (1u << 4)
} TxnQueryFilterType;

typedef uint32_t TxnQueryFilter;

typedef enum {
    REP_RESULT_DETAIL     = 0,
    REP_RESULT_AGGREGATED = 1,
    REP_RESULT_TXN
} TxnQueryResultMode_t;

typedef struct {
    uint32_t fdate;
    uint32_t ftime;
    uint32_t tdate;
    uint32_t ttime;
    uint64_t fdt;
    uint64_t tdt;
} QueryDateTimeFilter;

typedef struct {
    uint8_t              filter;
    TxnQueryResultMode_t resMode;
    char                 startDate[16];
    char                 endDate[16];
    char                 startTime[16];
    char                 endTime[16];
    char                 refNum[16];
    char                 trace[16];
    uint8_t              txnType;
} TxnQueryDsc;

bool queryFilterIsSet(TxnQueryFilter filter, TxnQueryFilterType flag);

void queryFilterSet(TxnQueryFilter* filter, TxnQueryFilterType flag);

void queryFilterClear(TxnQueryFilter* filter);

void applyFilter(TxnQueryFilterType flag, TxnQueryFilter filter,
                 QueryOperator* op, void* value);

#endif