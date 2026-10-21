#include "sys/sys.h"
#include "states/states.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "txn.h"
#include "logger.h"
#include "phrases/phrases.h"

int8_t getTxnName(TxnType type, char* name, size_t size) {
    RETURN_VALUE_IF_NULL(name, ;, ERR_BAD_PARAMETER);
    if (type > TXN_ALL)
        return ERR_BAD_PARAMETER;
    Phrases_t phrase;
    switch (type) {
    case TXN_PURCHASE:
        phrase = PHRASE_TXN_PURCHASE;
        break;
    case TXN_BALANCE:
        phrase = PHRASE_TXN_BALANCE;
        break;

    case TXN_BILL:
        phrase = PHRASE_TXN_BILL;
        break;

    case TXN_PAY:
        phrase = PHRASE_TXN_PAY;
        break;

    case TXN_TOPUP:
        phrase = PHRASE_TXN_TOPUP;
        break;

    case TXN_VOUCHER:
        phrase = PHRASE_TXN_VOUCHER;
        break;

    case TXN_CFG:
        phrase = PHRASE_TXN_CFG;
        break;

    default:
        break;
    }
    snprintf(name, size, "%s", phraseGetDef(phrase));
    return ERR_OK;
}

void logTxnCore(TxnData* data) {
    uint32_t idate, itime;
    unpackDateTime(&data->dateTime, &idate, &itime);
    LOG_TRACE("=============== Transaction core data ===============");
    LOG_TRACE("[## datetime    ##]    = %llu", data->dateTime);
    LOG_TRACE("[## date        ##]    = %lu", idate);
    LOG_TRACE("[## time        ##]    = %lu", itime);
    LOG_TRACE("[## status      ##]    = %d", data->status);
    LOG_TRACE("[## txnType     ##]    = %d", data->core.txnType);
    LOG_TRACE("[## mti         ##]    = %d", data->core.mti);
    LOG_TRACE("[## processCode ##]    = %d", data->core.processCode);
    LOG_TRACE("[## pan         ##]    = %s", data->core.pan);
    LOG_TRACE("[## amount      ##]    = %llu", data->core.amount);
    LOG_TRACE("[## rrn         ##]    = %llu", data->core.rrn);
    LOG_TRACE("[## trace       ##]    = %lu", data->core.trace);
    LOG_TRACE("[## stan        ##]    = %lu", data->core.stan);
    LOG_TRACE("[## respCode    ##]    = %lu", data->core.respCode);

    LOG_TRACE("=====================================================");
}