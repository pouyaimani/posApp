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
    LOG_TXN("=============== Transaction core data ===============");
    LOG_TXN("[## datetime    ##]    = %llu", data->dateTime);
    LOG_TXN("[## date        ##]    = %lu", idate);
    LOG_TXN("[## time        ##]    = %lu", itime);
    LOG_TXN("[## status      ##]    = %d", data->status);
    LOG_TXN("[## txnType     ##]    = %d", data->core.txnType);
    LOG_TXN("[## mti         ##]    = %d", data->core.mti);
    LOG_TXN("[## processCode ##]    = %d", data->core.processCode);
    LOG_TXN("[## pan         ##]    = %s", data->core.pan);
    LOG_TXN("[## amount      ##]    = %llu", data->core.amount);
    LOG_TXN("[## rrn         ##]    = %llu", data->core.rrn);
    LOG_TXN("[## trace       ##]    = %lu", data->core.trace);
    LOG_TXN("[## stan        ##]    = %lu", data->core.stan);
    LOG_TXN("[## respCode    ##]    = %lu", data->core.respCode);

    LOG_TXN("=====================================================");
}