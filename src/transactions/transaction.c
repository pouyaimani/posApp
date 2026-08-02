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

static const char* TxnTypeStr[] = {
    [TXN_PURCHASE] = "خرید",     [TXN_BILL] = "پرداخت قبض",
    [TXN_TOPUP] = "شارژ مستقیم", [TXN_BALANCE] = "موجودی",
    [TXN_PAY] = "پرداخت",        [TXN_VOUCHER] = "کد شارژ",
    [TXN_ALL] = "همه تراکنش ها"};

int8_t getTxnName(TxnType type, char* name, size_t size) {
    RETURN_VALUE_IF_NULL(name, ;, ERR_BAD_PARAMETER);
    if (type > TXN_ALL)
        return ERR_BAD_PARAMETER;
    snprintf(name, size, "%s", TxnTypeStr[type]);
    return ERR_OK;
}

static void createTxns() {
    parent             = (Transaction*)MEM_ALLOC(sizeof(Transaction));
    txns[TXN_PURCHASE] = (Purchase*)MEM_ALLOC(sizeof(Purchase));
    txns[TXN_BILL]     = (Bill*)MEM_ALLOC(sizeof(Bill));
    txns[TXN_BALANCE]  = (Balance*)MEM_ALLOC(sizeof(Balance));
    txns[TXN_PAY]      = (Pay*)MEM_ALLOC(sizeof(Pay));
    txns[TXN_VOUCHER]  = (Voucher*)MEM_ALLOC(sizeof(Voucher));
    txns[TXN_TOPUP]    = (TopUp*)MEM_ALLOC(sizeof(TopUp));

    OOP_CALL_CTOR(Transaction, parent, getState(STATE_ID_CARD_HOLDER),
                  "parent");
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