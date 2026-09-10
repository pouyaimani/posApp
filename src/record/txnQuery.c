#include "txnQuery.h"
#include "utility/utility.h"

bool queryFilterIsSet(TxnQueryFilter filter, TxnQueryFilterType flag) {
    return (filter & flag) != 0;
}

void queryFilterSet(TxnQueryFilter* filter, TxnQueryFilterType flag) {
    *filter |= flag;
}

void queryFilterClear(TxnQueryFilter* filter) { *filter = 0; }

void applyFilter(TxnQueryFilterType flag, TxnQueryFilter filter,
                 QueryOperator* op, void* value) {
    if (!queryFilterIsSet(filter, flag)) {
        return;
    }
    switch (flag) {
    case REP_FILTER_DATE_TIME: {
        QueryDateTimeFilter* dtfilter = (QueryDateTimeFilter*)value;
        dtfilter->fdt = packDateTime(dtfilter->fdate, dtfilter->ftime);
        dtfilter->tdt = packDateTime(dtfilter->tdate, dtfilter->ttime);
        txnquery()->where(op, TXN_REC_COL_KEY, SELECT_GTE, &dtfilter->fdt);
        txnquery()->where(op, TXN_REC_COL_KEY, SELECT_LTE, &dtfilter->tdt);
    } break;
    case REP_FILTER_TXN_TYPE:
        txnquery()->where(op, TXN_REC_COL_TYPE, SELECT_EQ, value);
        break;
    case REP_FILTER_TRACE:
        txnquery()->where(op, TXN_REC_COL_TRACE, SELECT_EQ, value);
        break;
    case REP_FILTER_REF_NUM:
        txnquery()->where(op, TXN_REC_COL_RRN, SELECT_EQ, value);
        break;
    case REP_FILTER_LATEST:
        txnquery()->limit(op,
                          (QueryLimit){.mode = QUERY_LIMIT_LATEST, .count = 1});
        break;

    default:
        break;
    }
}