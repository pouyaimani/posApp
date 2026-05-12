#include "receiptTemplates.h"
#include "common.h"
#include "settings/settings.h"
#include "utility/utility.h"

static int8_t receiptSectionPsp(Receipt *rec, const TxnData *txn) {
    // TODO: left side value?
    RecColumn_t row[] = {
        {"910990057", LV_TEXT_ALIGN_LEFT, 1},
        {"پرداخت سبز", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

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
    return ERR_OK;
}

static int8_t receiptSectionBankName(Receipt *rec, const TxnData *txn) {
        // TODO: bank name
    RecColumn_t row[] = {
        {"bank name", LV_TEXT_ALIGN_LEFT, 1},
        {"بانک", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
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
    return ERR_OK;
}

static int8_t receiptSectionBalance(Receipt *rec, const TxnData *txn) {
    DEFINE_STRING(balance, 64);
    snprintf(balance, sizeof(balance), "%s %d", txn->extention.balance.balance, "ریال");
    RecColumn_t row[] = {
        {balance, LV_TEXT_ALIGN_LEFT, 1},
        {"مانده واقعی", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);

    DEFINE_STRING(ledger, 64);
    snprintf(ledger, sizeof(ledger), "%s %d", txn->extention.balance.ledger, "ریال");
    RecColumn_t row1[] = {
        {ledger, LV_TEXT_ALIGN_LEFT, 1},
        {"مانده قابل برداشت", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionHeader(Receipt *rec, const TxnData *txn) {
    uint32_t date, time;
    DEFINE_STRING(dtStr, 24);
    unpackDateTime(txn->dateTime, &date, &time);
    dateTimeToStr(date, time, dtStr, sizeof(dtStr));
    DEFINE_STRING(header, 64);
    // TODO" service name
    snprintf(header, sizeof(header), "%s-%s-%s", dtStr, "رسید مشتری", "نام سرویس");
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionAmount(Receipt *rec, const TxnData *txn) {
    RecColumn_t row[] = {
        {txn->core.amount, LV_TEXT_ALIGN_LEFT, 1},
        {"عملیات موفق", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionFailure(Receipt *rec, const TxnData *txn) {
    DEFINE_STRING(desc, 24);
    // TODO: error description
    snprintf(desc, sizeof(desc), "%s (%d)", desc, "error description", txn->core.respCode);
    RecColumn_t row[] = {
        {desc, LV_TEXT_ALIGN_CENTER, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, 1, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionOperatorPhone(Receipt *rec, const TxnData *txn) {
    // TODO: operator name
    RecColumn_t row[] = {
        {"operator", LV_TEXT_ALIGN_LEFT, 1},
        {txn->extention.charge.phoneNumber, LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
}

static int8_t receiptSectionChargeCode(Receipt *rec, const TxnData *txn) {
    // TODO: operator name
    RecColumn_t row[] = {
        {txn->extention.charge.chargeSerial, LV_TEXT_ALIGN_LEFT, 1},
        {"سریال شارژ", LV_TEXT_ALIGN_RIGHT, 1}
    };
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, 2, row), ERR_OK, ; , ERR_NOK);
    RecColumn_t row1[] = {
        {txn->extention.charge.chargePin, LV_TEXT_ALIGN_LEFT, 1},
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

static int8_t buildSaleReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionHeader(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildBillReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildTopupReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionHeader(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionOperatorPhone(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildBalanceReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionHeader(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBalance(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildPaymentReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);    
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static int8_t buildChargeCodeReceipt(Receipt *rec, const TxnData *txn) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionHeader(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionRefTrace(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, txn), ERR_OK, ; , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ; , ERR_NOK);
    return ERR_OK;
}

static const ReceiptTemplate templates[] = {
    { TXN_SALE,                 buildSaleReceipt     },
    { TXN_BILL,                 buildBillReceipt     },
    { TXN_TOPUP,                buildTopupReceipt    },
    { TXN_BALANCE,              buildBalanceReceipt  },
    { TXN_PAY,                  buildPaymentReceipt  },
    { TXN_SIM_CHARGE_CODE,      buildChargeCodeReceipt  }
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