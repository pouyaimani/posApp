#include "transaction.h"
#include "sys/sys.h"
#include "states/states.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "txn.h"
#include "logger.h"
#include "phrases/phrases.h"

static Transaction* txns[LEN_MAX_SERVICE_NUM];
static Transaction* parent;
static TxnFlow      flow;

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

static void createTxns() {
    parent             = (Transaction*)MEM_ALLOC(sizeof(Transaction));
    txns[TXN_LOGON]    = (Logon*)MEM_ALLOC(sizeof(Logon));
    txns[TXN_CFG]      = (Config*)MEM_ALLOC(sizeof(Config));
    txns[TXN_PURCHASE] = (Purchase*)MEM_ALLOC(sizeof(Purchase));
    txns[TXN_BILL]     = (Bill*)MEM_ALLOC(sizeof(Bill));
    txns[TXN_BALANCE]  = (Balance*)MEM_ALLOC(sizeof(Balance));
    txns[TXN_PAY]      = (Pay*)MEM_ALLOC(sizeof(Pay));
    txns[TXN_VOUCHER]  = (Voucher*)MEM_ALLOC(sizeof(Voucher));
    txns[TXN_TOPUP]    = (TopUp*)MEM_ALLOC(sizeof(TopUp));

    OOP_CALL_CTOR(Transaction, parent, getState(STATE_ID_CARD_HOLDER),
                  "parent");
    OOP_CALL_CTOR(Logon, txns[TXN_LOGON], parent,
                  phraseGetDef(PHRASE_TXN_LOGON));
    OOP_CALL_CTOR(Config, txns[TXN_CFG], parent, phraseGetDef(PHRASE_TXN_CFG));
    OOP_CALL_CTOR(Purchase, txns[TXN_PURCHASE], parent,
                  phraseGetDef(PHRASE_TXN_PURCHASE));
    OOP_CALL_CTOR(Bill, txns[TXN_BILL], parent, phraseGetDef(PHRASE_TXN_BILL));
    OOP_CALL_CTOR(Balance, txns[TXN_BALANCE], parent,
                  phraseGetDef(PHRASE_TXN_BALANCE));
    OOP_CALL_CTOR(Pay, txns[TXN_PAY], parent, phraseGetDef(PHRASE_TXN_PAY));
    OOP_CALL_CTOR(Voucher, txns[TXN_VOUCHER], parent,
                  phraseGetDef(PHRASE_TXN_VOUCHER));
    OOP_CALL_CTOR(TopUp, txns[TXN_TOPUP], parent,
                  phraseGetDef(PHRASE_TXN_TOPUP));
}

Transaction* getTxn(TxnType id) {
    CALL_ONCE(createTxns(););
    return txns[id];
}

OOP_CTOR(Transaction, State* parent, const char* name) {
    OOP_CALL_CTOR(State, &self->state, parent, name);
    self->flow = &flow;
    snprintf(self->name, sizeof(self->name), "%s", name);
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