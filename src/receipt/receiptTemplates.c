#include "receiptTemplates.h"
#include "common.h"
#include "settings/settings.h"
#include "utility/utility.h"

static int8_t receiptSectionTerminalInfo(Receipt *rec, const TxnData *txn) {
    DEFINE_STRING(terminal, 64);
    snprintf(terminal, sizeof(terminal), "%s:%s", settings()->terminal.terminalNo, "پایانه");
    DEFINE_STRING(code, 64);
    snprintf(code, sizeof(code), "%s:%s", settings()->terminal.terminalNo, "کد کارتخوان");
    RecColumn_t row[] = {
        {terminal, LV_TEXT_ALIGN_LEFT, 1},
        {code, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionBankName(Receipt *rec, const TxnData *txn) {
        // TODO: bank name
    RecColumn_t row[] = {
        {"bank name", LV_TEXT_ALIGN_LEFT, 1},
        {"بانک", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionRefTrace(Receipt *rec, const TxnData *txn) {
    DEFINE_STRING(trace, 64);
    snprintf(trace, sizeof(trace), "%s:%d", txn->core.trace, "پیگیری");
    DEFINE_STRING(ref, 64);
    snprintf(ref, sizeof(ref), "%s:%d", txn->core.refNum, "مرجع");
    RecColumn_t row[] = {
        {ref, LV_TEXT_ALIGN_LEFT, 1},
        {trace, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionHeader(Receipt *rec, const TxnData *txn) {
    uint32_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionAmount(Receipt *rec, const TxnData *txn) {
}

static int8_t buildSaleReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
}

static int8_t buildBillReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK); 
}

static int8_t buildTopupReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);    
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
}

static const ReceiptTemplate templates[] = {
    { TXN_SALE,     buildSaleReceipt },
    { TXN_BILL,     buildBillReceipt     },
    { TXN_TOPUP,    buildTopupReceipt    },
};

int8_t buildReceipt(
    Receipt *rec,
    TxnType_t type,
    const TxnData *txn)
{
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(createReceipt(&rec), true, 
        RECEIPT_CREATE_ERROR(), ERR_MEMORY_ALLOCATION);
    size_t i;

    for (i = 0; i < ARRAY_SIZE(templates); i++) {

        if (templates[i].type == type) {
            return templates[i].builder(rec, txn);
        }
    }

    return ERR_NOT_SUPPORTED;
}