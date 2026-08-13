#include "receiptTemplates.h"
#include "common.h"
#include "settings/settings.h"
#include "utility/utility.h"
#include "sys/sys.h"
#include "phrases/phrases.h"
#include "logger.h"
#include "ui/digitalReceipt.h"
#include "assets.h"
#include "mylvgl.h"
#include "ped/ped.h"
#include "cellular/cellular.h"

static int8_t receiptSectionPsp(Receipt* rec) {
    RecColumn_t row[] = {
        {settings()->terminal.merchantPhone, LV_TEXT_ALIGN_LEFT, 1},
        {settings()->terminal.merchantName, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionTerminalInfo(Receipt* rec) {
    DEFINE_STRING(terminal, 64);
    snprintf(terminal, sizeof(terminal), "%s:%s", phraseGetDef(PHRASE_TERMINAL),
             settings()->terminal.terminalId);
    DEFINE_STRING(code, 64);
    snprintf(code, sizeof(code), "%s:%s", phraseGetDef(PHRASE_DEVICE_CODE),
             settings()->terminal.terminalId);
    RecColumn_t row[] = {{code, LV_TEXT_ALIGN_LEFT, 1},
                         {terminal, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionBankName(Receipt* rec, const char* pan) {
    RETURN_VALUE_IF_NULL(pan, ;, ERR_NOK);
    LOG_TRACE("Receipt: extracting bank name for pan = %s", pan);
    DEFINE_STRING(bin, LEN_MAX_BIN + 1);
    extractBin(pan, bin, sizeof(bin), LEN_MAX_BIN);
    DEFINE_STRING(maskedPan, LEN_MAX_PAN + 1);
    maskPan(pan, maskedPan, sizeof(maskedPan));
    RecColumn_t row[] = {{maskedPan, LV_TEXT_ALIGN_LEFT, 1},
                         {bankNameGetDef(bin), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionRefTrace(Receipt* rec, const uint32_t* trace,
                                     const uint32_t* refNum) {
    DEFINE_STRING(traceStr, 64);
    snprintf(traceStr, sizeof(traceStr), "%s:%d", phraseGetDef(PHRASE_TRACE),
             *trace);
    DEFINE_STRING(refStr, 64);
    snprintf(refStr, sizeof(refStr), "%s:%d", phraseGetDef(PHRASE_REFERENCE),
             *refNum);
    RecColumn_t row[] = {{refStr, LV_TEXT_ALIGN_LEFT, 1},
                         {traceStr, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionBalance(Receipt* rec, const uint64_t* balance,
                                    const uint64_t* ledger) {
    DEFINE_STRING(balanceStr, 64);
    DEFINE_STRING(balanceSep, 32);

    snprintf(balanceStr, sizeof(balanceStr), "%llu", *balance);
    amountSeparator(balanceStr, balanceSep, sizeof(balanceSep));
    snprintf(balanceStr, sizeof(balanceStr), "%s %s", balanceSep,
             phraseGetDef(PHRASE_RIAL));
    RecColumn_t row[] = {{balanceStr, LV_TEXT_ALIGN_LEFT, 1},
                         {phraseGetDef(PHRASE_LEDGER), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 2, row), ERR_OK, ;
        , ERR_NOK);

    DEFINE_STRING(ledgerStr, 64);
    DEFINE_STRING(ledgerSep, 32);
    snprintf(ledgerStr, sizeof(ledgerStr), "%llu", *ledger);
    amountSeparator(ledgerStr, ledgerSep, sizeof(ledgerSep));
    snprintf(ledgerStr, sizeof(ledgerStr), "%s %s", ledgerSep,
             phraseGetDef(PHRASE_RIAL));
    RecColumn_t row1[] = {
        {ledgerStr, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_AVAILABLE), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addText, REC_FONT_BOLD, 2, row1), ERR_OK,
                        ;, ERR_NOK);
    // RETURN_VALUE_IF_NOT(
    //     OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row1), ERR_OK, ;,
    //     ERR_NOK);
    return ERR_OK;
}

static int8_t receiptSectionTxnHeader(Receipt* rec, const uint64_t* dateTime,
                                      TxnType type) {
    uint32_t idate, itime;
    unpackDateTime(dateTime, &idate, &itime);
    DEFINE_STRING(dateStr, 24);
    DEFINE_STRING(timeStr, 24);
    dateToJalaliStr(idate, dateStr, sizeof(dateStr));
    timeToStr(itime, timeStr, sizeof(timeStr));

    DEFINE_STRING(header, 64);
    DEFINE_STRING(dt, 32);
    DEFINE_STRING(servName, 32);
    getTxnName(type, servName, sizeof(servName));
    char* customerRec = "";
    // TODO: use config to wether print this or not
    if (type != TXN_LOGON && type != TXN_CFG) {
        snprintf(header, sizeof(header), "%s-%s", servName,
                 phraseGetDef(PHRASE_CUSTOMER_RECEIPT));
    } else {
        snprintf(header, sizeof(header), "%s", servName);
    }
    snprintf(dt, sizeof(dt), "%s-%s", dateStr, timeStr);
    RecColumn_t row[] = {{dt, LV_TEXT_ALIGN_LEFT, 1},
                         {header, LV_TEXT_ALIGN_RIGHT, 1}};
    return OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 2, row);
}

static int8_t receiptSectionAmount(Receipt* rec, const uint64_t* amount) {
    DEFINE_STRING(amountStr, 36);
    DEFINE_STRING(amountSep, 24);
    DEFINE_STRING(txt, 128);
    snprintf(amountStr, sizeof(amountStr), "%llu", *amount);
    amountSeparator(amountStr, amountSep, sizeof(amountSep));
    memset(amountStr, 0, sizeof(amountStr));
    snprintf(txt, sizeof(txt), "%s %s", amountSep, phraseGetDef(PHRASE_RIAL));
    RecColumn_t row[] = {
        {txt, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SUCCESSFUL_OPERATION), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, REC_FONT_BOLD, 2, row),
                        ERR_OK,
                        ;, ERR_NOK);
}

static int8_t receiptSectionFailure(Receipt* rec, uint16_t respCode) {
    DEFINE_STRING(dsc, 128);
    getResponseCode(respCode, dsc, sizeof(dsc));
    DEFINE_STRING(fdsc, 128);
    snprintf(fdsc, sizeof(fdsc), "%s (%d)", dsc, respCode);
    RecColumn_t row[] = {{fdsc, LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addTextWithBorder, REC_FONT_BOLD, 1, row),
                        ERR_OK,
                        ;, ERR_NOK);
}

static int8_t receiptSectionOperatorPhone(Receipt* rec, const char* phone,
                                          SimCardOp_t op) {
    DEFINE_STRING(opDsc, 32);
    getSimOpName(op, opDsc, sizeof(opDsc));
    RecColumn_t row[] = {{opDsc, LV_TEXT_ALIGN_LEFT, 1},
                         {phone, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
}

static int8_t receiptSectionChargeCode(Receipt* rec, const char* serial,
                                       const char* pin, SimCardOp_t op) {
    RecColumn_t row[] = {
        {serial, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SIM_CHARGE_SERIAL), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    RecColumn_t row1[] = {
        {pin, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SIM_CHARGE_PIN), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    DEFINE_STRING(command, 16);
    snprintf(command, sizeof(command), "%s %s", "", "*رمز شارژ#");
    DEFINE_STRING(opDsc, 32);
    getSimOpName(op, opDsc, sizeof(opDsc));
    RecColumn_t row2[] = {{command, LV_TEXT_ALIGN_LEFT, 1},
                          {opDsc, LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
}

static int8_t buildPurchaseReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, data->txn.core.pan), ERR_OK,
                        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.rrn), ERR_OK, ;
        , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildBillReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildTopupReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, data->txn.core.pan), ERR_OK,
                        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.rrn), ERR_OK, ;
        , ERR_NOK);

    RETURN_VALUE_IF_NOT(
        receiptSectionOperatorPhone(rec, txn->extention.charge.phoneNumber,
                                    txn->extention.charge.op),
        ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildBalanceReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, data->txn.core.pan), ERR_OK,
                        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.rrn), ERR_OK, ;
        , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBalance(rec,
                                              &txn->extention.balance.available,
                                              &txn->extention.balance.ledger),
                        ERR_OK,
                        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildPaymentReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildChargeCodeReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, data->txn.core.pan), ERR_OK,
                        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.rrn), ERR_OK, ;
        , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}
static int8_t buildLogonReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RecColumn_t row[] = {
        {phraseGetDef(PHRASE_KEYS_KCV), LV_TEXT_ALIGN_CENTER, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 1, row), ERR_OK, ;
        , ERR_NOK);
    DEFINE_STRING(kcv, 8);
    DEFINE_STRING(skcv, 8);
    // KCV Master
    OOP_CALL(ped(), getKcv, PED_KEY_MASTER, kcv, sizeof(kcv));
    sprintf(skcv, "%02X-%02X-%02X-%02X", kcv[0], kcv[1], kcv[2], kcv[3]);
    RecColumn_t row1[] = {
        {skcv, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_KEY_MASTER), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    RESET_STRING(kcv);
    RESET_STRING(skcv);

    // KCV Pin
    OOP_CALL(ped(), getKcv, PED_KEY_PIN, kcv, sizeof(kcv));
    sprintf(skcv, "%02X-%02X-%02X-%02X", kcv[0], kcv[1], kcv[2], kcv[3]);
    RecColumn_t row2[] = {
        {skcv, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_KEY_PIN), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    RESET_STRING(kcv);
    RESET_STRING(skcv);

    // KCV Mac
    OOP_CALL(ped(), getKcv, PED_KEY_MAC, kcv, sizeof(kcv));
    sprintf(skcv, "%02X-%02X-%02X-%02X", kcv[0], kcv[1], kcv[2], kcv[3]);
    RecColumn_t row3[] = {
        {skcv, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_KEY_MAC), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    RESET_STRING(kcv);
    RESET_STRING(skcv);

    // KCV Data
    OOP_CALL(ped(), getKcv, PED_KEY_DATA, kcv, sizeof(kcv));
    sprintf(skcv, "%02X-%02X-%02X-%02X", kcv[0], kcv[1], kcv[2], kcv[3]);
    RecColumn_t row4[] = {
        {skcv, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_KEY_PIN), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addLineHorizontal, 2, 2, 2), ERR_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildNetSpec(Receipt* rec, const ReceiptData* data) {
    VAR_UNUSED(data);
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);

    RecColumn_t row[] = {
        {phraseGetDef(PHRASE_NET_SPECS), LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 1, row), ERR_OK, ;
        , ERR_NOK);

    DEFINE_STRING(route, 32);
    getCurrentNetRouteName(route, sizeof(route));
    RecColumn_t row1[] = {
        {route, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_CONNECTION_TYPE), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row1), ERR_OK, ;, ERR_NOK);

    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    DEFINE_STRING(ipPort, 32);
    snprintf(ipPort, sizeof(ipPort), "%s : %d", ip,
             settings()->server.mainServerPort);
    RecColumn_t row2[] = {
        {ipPort, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_MAIN_SERVER), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row2), ERR_OK, ;, ERR_NOK);

    RESET_STRING(ip);
    RESET_STRING(ipPort);
    char* tmsIpPort;
    if (isStringEmpty(settings()->server.tmsIp)) {
        tmsIpPort = phraseGetDef(PHRASE_IS_NOT_SET);
    } else {
        normalizeIp(settings()->server.tmsIp, ip, sizeof(ip));
        snprintf(ipPort, sizeof(ipPort), "%s : %d", ip,
                 settings()->server.tmsPort);
        tmsIpPort = ipPort;
    }
    RecColumn_t row3[] = {
        {tmsIpPort, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_APP_UPDATE_SERVER), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row3), ERR_OK, ;, ERR_NOK);

    DEFINE_STRING(imei, 48);
    OOP_CALL(cellular(), getImei, imei, sizeof(imei));
    RecColumn_t row4[] = {{imei, LV_TEXT_ALIGN_LEFT, 1},
                          {phraseGetDef(PHRASE_IMEI), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row4), ERR_OK, ;, ERR_NOK);
}

static int8_t buildCfgReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionTxnHeader(rec, &txn->dateTime, txn->core.txnType), ERR_OK,
        ;, ERR_NOK);
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    RecColumn_t row1[] = {
        {sn, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_SERIAL), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row1), ERR_OK, ;, ERR_NOK);

    DEFINE_STRING(terminal, 64);
    snprintf(terminal, sizeof(terminal), "%s:%s", phraseGetDef(PHRASE_TERMINAL),
             settings()->terminal.terminalId);
    DEFINE_STRING(merchant, 64);
    snprintf(merchant, sizeof(merchant), "%s:%s", phraseGetDef(PHRASE_MERCHANT),
             settings()->terminal.merchantId);
    RecColumn_t row2[] = {{terminal, LV_TEXT_ALIGN_LEFT, 1},
                          {merchant, LV_TEXT_ALIGN_RIGHT, 2}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row2), ERR_OK, ;, ERR_NOK);

    RecColumn_t row3[] = {
        {PNA_APP_VERSION, LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_VERSION), LV_TEXT_ALIGN_RIGHT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row3), ERR_OK, ;, ERR_NOK);

    RETURN_VALUE_IF_NOT(buildNetSpec(rec, data), ERR_OK, ;, ERR_NOK);

    RETURN_VALUE_IF_NOT(OOP_CALL(rec, addFooter), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDailyRepHeaderReceipt(Receipt*           rec,
                                         const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    DailyReportHeader* header = &data->dailyHeader;
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    DEFINE_STRING(txt, 36);
    snprintf(txt, sizeof(txt), "%s %s", phraseGetDef(PHRASE_DAILY_REPORT),
             phraseGetDef(PHRASE_TXN_ALL));
    RecColumn_t row[] = {
        {"گزارش روزانه همه تراکنش ها", LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 1, row), ERR_OK, ;
        , ERR_NOK);
    // Date Time
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDailyRepBodyReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    uint32_t date, time;
    DEFINE_STRING(dtStr, 24);
    unpackDateTime(txn->dateTime, &date, &time);
    dateTimeToStr(date, time, dtStr, sizeof(dtStr));
    // TODO" service name
    DEFINE_STRING(title, 32);
    snprintf(title, sizeof(title), "%s-%s", dtStr,
             phraseGetDef(PHRASE_SERVICE_NAME));
    RecColumn_t row[] = {{title, LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 1, row), ERR_OK, ;
        , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionBankName(rec, data->txn.core.pan), ERR_OK,
                        ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(
        receiptSectionRefTrace(rec, &txn->core.trace, &txn->core.rrn), ERR_OK, ;
        , ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionAmount(rec, &txn->core.amount), ERR_OK, ;
                        , ERR_NOK);
    return ERR_OK;
}

static int8_t buildSumRepHeaderReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    SummaryReportHeader* header = &data->summaryHeader;
    RecColumn_t          row[]  = {
        {phraseGetDef(PHRASE_SUMMARY_REPORT), LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 1, row), ERR_OK, ;
        , ERR_NOK);
    DATE_TIME_STR(dt);
    dateTimeToStr(header->dateNow, header->timeNow, dt, sizeof(dt));
    DEFINE_STRING(day, 24);
    uint32_t date = OOP_CALL(sys(), getDate);
    getNameofDay(date, day, sizeof(day));
    RecColumn_t row1[] = {
        {day, LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row1), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateFrom, header->timeFrom, dt, sizeof(dt));
    RecColumn_t row2[] = {
        {phraseGetDef(PHRASE_FROM_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row2), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateTo, header->timeTo, dt, sizeof(dt));
    RecColumn_t row3[] = {
        {phraseGetDef(PHRASE_TO_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row3), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RecColumn_t row4[] = {{phraseGetDef(PHRASE_TO_RIAL), LV_TEXT_ALIGN_LEFT, 1},
                          {phraseGetDef(PHRASE_COUNT), LV_TEXT_ALIGN_LEFT, 1},
                          {phraseGetDef(PHRASE_TXN), LV_TEXT_ALIGN_LEFT, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTable, REC_FONT_REGULAR, 3, row4), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildSumRepBodyReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    SummaryReportBody* body = &data->summaryBody;
    DEFINE_STRING(countStr, 16);
    snprintf(countStr, sizeof(countStr), "%u", body->count);
    AMOUNT_STR(amntStr);
    snprintf(amntStr, sizeof(amntStr), "%llu", body->amntSum);
    DEFINE_STRING(amntStrSep, 24);
    amountSeparator(amntStr, amntStrSep, sizeof(amntStrSep));
    DEFINE_STRING(txnName, 64);
    getTxnName(body->txnType, txnName, sizeof(txnName));
    RecColumn_t row[] = {
        {amntStr, LV_TEXT_ALIGN_LEFT, 1},
        {countStr, LV_TEXT_ALIGN_LEFT, 1},
        {txnName, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTable, REC_FONT_REGULAR, 3, row), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailtRepHeaderReceipt(Receipt*           rec,
                                           const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(receiptSectionPsp(rec), ERR_OK, ;, ERR_NOK);
    DetailedReportHeader* header = &data->detailedHeader;
    DEFINE_STRING(txt, 36);
    snprintf(txt, sizeof(txt), "%s %s", phraseGetDef(PHRASE_DETAILED_REPORT),
             phraseGetDef(PHRASE_TXN_ALL));
    RecColumn_t row[] = {{txt, LV_TEXT_ALIGN_CENTER, 1}};
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTextWithBorder, REC_FONT_REGULAR, 1, row), ERR_OK, ;
        , ERR_NOK);
    DATE_TIME_STR(dt);
    dateTimeToStr(header->dateNow, header->timeNow, dt, sizeof(dt));
    uint32_t date = OOP_CALL(sys(), getDate);
    DEFINE_STRING(today, 24);
    getNameofDay(date, today, sizeof(today));
    RecColumn_t row1[] = {
        {today, LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row1), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateFrom, header->timeFrom, dt, sizeof(dt));
    RecColumn_t row2[] = {
        {phraseGetDef(PHRASE_FROM_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row2), ERR_OK, ;, ERR_NOK);
    dateTimeToStr(header->dateTo, header->timeTo, dt, sizeof(dt));
    RecColumn_t row3[] = {
        {phraseGetDef(PHRASE_TO_DATE), LV_TEXT_ALIGN_RIGHT, 1},
        {dt, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addText, REC_FONT_REGULAR, 2, row3), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(receiptSectionTerminalInfo(rec), ERR_OK, ;, ERR_NOK);
    RecColumn_t row4[] = {
        {phraseGetDef(PHRASE_DATE), LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_TRACE), LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_TO_RIAL), LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_TXN), LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTable, REC_FONT_REGULAR, 4, row4), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailRepBodyReceipt(Receipt* rec, const ReceiptData* data) {
    RETURN_VALUE_IF_NULL(rec, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(data, ;, ERR_BAD_PARAMETER);
    TxnData* txn = &data->txn;
    DATE_TIME_STR(dt);
    uint16_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    shortDateTimeToStr(date, time, dt, sizeof(dt));
    DEFINE_STRING(traceStr, 16);
    snprintf(traceStr, sizeof(traceStr), "%u", txn->core.trace);
    AMOUNT_STR(amntStr);
    snprintf(amntStr, sizeof(amntStr), "%llu", txn->core.amount);
    DEFINE_STRING(amntStrSep, 24);
    amountSeparator(amntStr, amntStrSep, sizeof(amntStrSep));
    DEFINE_STRING(txnName, 64);
    getTxnName(txn->core.txnType, txnName, sizeof(txnName));
    RecColumn_t row4[] = {
        {dt, LV_TEXT_ALIGN_LEFT, 1},
        {traceStr, LV_TEXT_ALIGN_LEFT, 1},
        {amntStr, LV_TEXT_ALIGN_LEFT, 1},
        {txnName, LV_TEXT_ALIGN_LEFT, 1},
    };
    RETURN_VALUE_IF_NOT(
        OOP_CALL(rec, addTable, REC_FONT_REGULAR, 4, row4), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static int8_t buildDetailRepReceipt(Receipt* rec, const ReceiptData* data) {}

static int8_t buildDailyRepReceipt(Receipt* rec, const ReceiptData* data) {}

static int8_t buildSumRepReceipt(Receipt* rec, const ReceiptData* data) {}

static void handleError(Result_t res) {
    if (res.err == ERR_DSC_PRINTER) {
        switch (res.detail.printer) {
        case PRNT_ERR_NO_PAPER:
            LOG_FATAL("Receipt: Printer has no paper.");
            break;
        case PRNT_ERR_OVER_HEAT:
            LOG_FATAL("Receipt: Printer pverheat.");
            break;
        case PRNT_ERR_TIME_OUT:
            LOG_FATAL("Receipt: Printer timeout.");
            break;
        case PRNT_ERR_NOK:
            LOG_FATAL("Receipt: Printer unknown error.");
            break;

        default:
            break;
        }
    }
}

Result_t buildReceipt(Receipt* rec, const ReceiptData* data) {
    Result_t res;
    RETURN_VALUE_IF_NULL(rec, res.err = ERR_DSC_INVALID_ARG;, res);
    RETURN_VALUE_IF_NULL(data, res.err = ERR_DSC_INVALID_ARG;, res);
    RETURN_VALUE_IF_NOT(createReceipt(rec), ERR_OK, res.err = ERR_DSC_MEMORY;
                        , res);
    ReceiptBuilder builder = NULL;
    if (data->type == DOC_TXN) {
        switch (data->txn.core.txnType) {
        case TXN_LOGON:
            builder = buildLogonReceipt;
            break;
        case TXN_CFG:
            builder = buildCfgReceipt;
            break;
        case TXN_PURCHASE:
            builder = buildPurchaseReceipt;
            break;
        case TXN_BILL:
            builder = buildBillReceipt;
            break;
        case TXN_TOPUP:
            builder = buildTopupReceipt;
            break;
        case TXN_BALANCE:
            builder = buildBalanceReceipt;
            break;
        case TXN_PAY:
            builder = buildPaymentReceipt;
            break;
            // case TXN_SIM_CHARGE:
            //     builder = buildChargeCodeReceipt;
            break;
        default:
            break;
        }
    } else {
        switch (data->type) {
        case DOC_DAILY_REPORT:
            builder = buildDailyRepReceipt;
            break;
        case DOC_SUMMARY_REPORT:
            builder = buildSumRepReceipt;
            break;
        case DOC_DETAILED_REPORT:
            builder = buildDetailRepReceipt;
            break;
        default:
            break;
        }
    }

    RETURN_VALUE_IF_NULL(builder, res.err = ERR_DSC_NOT_SUPPORTED;, res);
    RETURN_VALUE_IF_NOT(builder(rec, data), ERR_OK, ;, res);
    res = OOP_CALL(rec, flush);
    if (res.err != ERR_DSC_OK) {
        handleError(res);
    }
    OOP_CALL(rec, destroy);
    return res;
}

/*********************************************************************************************
 *                                                                                           *
 *                                      Digital Receipt
 *                                                                                           *
 ********************************************************************************************/

static ReceiptPage digitalReceipt;

static ReceiptHeader  header;
static ReceiptDetails details;
static ReceiptButtons buttons;

static void home_cb(lv_event_t* e) {
    LV_UNUSED(e);
    printf("Home clicked\n");
}

static void print_cb(lv_event_t* e) {
    LV_UNUSED(e);
    printf("Print clicked\n");
}

void createDigitalRec() {
    header  = (ReceiptHeader){.dateTitle = "تاریخ",
                              .date      = "",

                              .timeTitle = "ساعت",
                              .time      = "",

                              .amount   = "",
                              .currency = "ریال",

                              .statusTitle       = "تراکنش موفق",
                              .statusDescription = "",

                              .statusIcon = ICON_SUCCEED};
    details = (ReceiptDetails){
        .count = 4,

        .details = {{.icon       = ICON_TERMINAL,
                     .title      = "کد کارتخوان",
                     .value      = "",
                     .valueColor = lv_palette_main(LV_PALETTE_RED)},

                    {.icon       = ICON_DOC1,
                     .title      = " پیگیری/مرجع",
                     .value      = "",
                     .valueColor = lv_color_black()},

                    {.icon       = ICON_BANK,
                     .title      = "بانک سامان",
                     .value      = "6219000000000000",
                     .valueColor = lv_palette_main(LV_PALETTE_RED)},

                    {.icon       = ICON_DOC2,
                     .title      = "خطا             ",
                     .value      = "رمز نامعتبر است",
                     .valueColor = lv_color_black()}}};

    buttons = (ReceiptButtons){.left = {.text     = "صفحه اصلی",
                                        .icon     = "",
                                        .callback = home_cb,
                                        .primary  = false},

                               .right = {.text     = "چاپ رسید",
                                         .icon     = "",
                                         .callback = print_cb,
                                         .primary  = true}};
    ui_receipt_create(&digitalReceipt, &header, &details, &buttons);
}

static void DigRecfillCommon(const TxnData* data) {
    uint32_t idate, itime;
    unpackDateTime(&data->dateTime, &idate, &itime);
    DEFINE_STRING(dateStr, 24);
    DEFINE_STRING(timeStr, 24);
    dateToJalaliStr(idate, dateStr, sizeof(dateStr));
    timeToStr(itime, timeStr, sizeof(timeStr));

    LOG_TRACE("packed dt = %llu, date = %lu, time = %lu, date str "
              "= %s, time str = %s",
              data->dateTime, idate, itime, dateStr, timeStr);

    lv_palette_t palette =
        data->core.respCode == 0 ? LV_PALETTE_GREEN : LV_PALETTE_RED;
    header.statusIcon = data->core.respCode == 0 ? ICON_SUCCEED : ICON_FAILED;
    header.statusTitle =
        phraseGetDef(data->core.respCode == 0 ? PHRASE_SUCCESSFUL_TXN
                                              : PHRASE_UNSUCCESSFUL_TXN);

    details.count = 3;
    DEFINE_STRING(bin, LEN_MAX_BIN + 1);
    extractBin(data->core.pan, bin, sizeof(bin), LEN_MAX_BIN);
    DEFINE_STRING(maskedPan, LEN_MAX_PAN + 1);
    maskPan(data->core.pan, maskedPan, sizeof(maskedPan));

    DEFINE_STRING(refTrace, 64);
    snprintf(refTrace, sizeof(refTrace), "%llu - %llu", data->core.rrn,
             data->core.trace);

    // Header
    lv_label_set_text(digitalReceipt.date, dateStr);
    lv_label_set_text(digitalReceipt.time, timeStr);

    lv_obj_set_style_text_color(digitalReceipt.statusTitle,
                                lv_palette_main(palette), 0);
    lv_obj_set_style_text_color(digitalReceipt.amount, lv_palette_main(palette),
                                0);
    lv_label_set_text(digitalReceipt.statusTitle, header.statusTitle);
    lv_img_set_src(digitalReceipt.statusIcon, header.statusIcon);

    lv_label_set_text(digitalReceipt.detailValue[0],
                      settings()->terminal.terminalId);
    lv_label_set_text(digitalReceipt.detailValue[1], refTrace);
    lv_label_set_text(digitalReceipt.detailTitle[2], bankNameGetDef(bin));
    lv_label_set_text(digitalReceipt.detailValue[2], maskedPan);
    return ERR_OK;
}

static int8_t digRecBuildFailure(const TxnData* data) {
    details.count++;
    DEFINE_STRING(dsc, 128);
    getResponseCode(data->core.respCode, dsc, sizeof(dsc));
    lv_label_set_text(digitalReceipt.amount, "");
    lv_label_set_text_fmt(digitalReceipt.detailTitle[3], "%s (%d)",
                          phraseGetDef(PHRASE_ERROR), data->core.respCode);
    lv_label_set_text(digitalReceipt.detailValue[3], dsc);
    return ERR_OK;
}

static int8_t digRecBuildPurchase(const TxnData* data) {
    DEFINE_STRING(amountStr, 24);
    DEFINE_STRING(amountSep, 24);
    snprintf(amountStr, sizeof(amountStr), "%llu", data->core.amount);
    amountSeparator(amountStr, amountSep, sizeof(amountSep));
    lv_label_set_text_fmt(digitalReceipt.amount, "%s %s", amountSep,
                          header.currency);
    return ERR_OK;
}

static int8_t digRecBuildTopUp(const TxnData* data) {
    DEFINE_STRING(amountStr, 24);
    DEFINE_STRING(amountSep, 24);
    snprintf(amountStr, sizeof(amountStr), "%llu", data->core.amount);
    amountSeparator(amountStr, amountSep, sizeof(amountSep));
    lv_label_set_text_fmt(digitalReceipt.amount, "%s %s", amountSep,
                          header.currency);
    return ERR_OK;
}

static int8_t digRecBuildVoucher(const TxnData* data) {
    DEFINE_STRING(amountStr, 24);
    DEFINE_STRING(amountSep, 24);
    snprintf(amountStr, sizeof(amountStr), "%llu", data->core.amount);
    amountSeparator(amountStr, amountSep, sizeof(amountSep));
    lv_label_set_text_fmt(digitalReceipt.amount, "%s %s", amountSep,
                          header.currency);
    return ERR_OK;
}

static int8_t digRecBuildBill(const TxnData* data) {
    DEFINE_STRING(amountStr, 24);
    DEFINE_STRING(amountSep, 24);
    snprintf(amountStr, sizeof(amountStr), "%llu", data->core.amount);
    amountSeparator(amountStr, amountSep, sizeof(amountSep));
    lv_label_set_text_fmt(digitalReceipt.amount, "%s %s", amountSep,
                          header.currency);
    return ERR_OK;
}

static int8_t digRecBuildBalance(const TxnData* data) {
    details.count++;
    // details.details[3].value = "";
    DEFINE_STRING(availableStr, 36);
    DEFINE_STRING(availableSep, 24);
    snprintf(availableStr, sizeof(availableStr), "%llu",
             data->extention.balance.available);
    amountSeparator(availableStr, availableSep, sizeof(availableSep));
    DEFINE_STRING(ledgerStr, 36);
    DEFINE_STRING(ledgerSep, 24);
    snprintf(ledgerStr, sizeof(ledgerStr), "%llu",
             data->extention.balance.ledger);
    amountSeparator(ledgerStr, ledgerSep, sizeof(ledgerSep));

    lv_label_set_text_fmt(digitalReceipt.amount, "%s %s", availableSep,
                          header.currency);
    lv_label_set_text(digitalReceipt.statusTitle,
                      phraseGetDef(PHRASE_AVAILABLE));
    lv_label_set_text(digitalReceipt.detailTitle[3],
                      phraseGetDef(PHRASE_LEDGER));
    lv_label_set_text(digitalReceipt.detailValue[3], ledgerSep);
    return ERR_OK;
}

Result_t showDigitalRec(const TxnData* data) {
    CALL_ONCE(createDigitalRec(););
    DigRecfillCommon(data);
    DigitalReceiptBuilder builder = NULL;
    switch (data->core.txnType) {
    case TXN_PURCHASE:
        builder = digRecBuildPurchase;
        break;
    case TXN_BILL:
        builder = digRecBuildBill;
        break;
    case TXN_TOPUP:
        builder = digRecBuildTopUp;
        break;
    case TXN_BALANCE:
        builder = digRecBuildBalance;
        break;
    case TXN_PAY:
        break;
    case TXN_VOUCHER:
        builder = digRecBuildVoucher;
        break;
    default:
        break;
    }
    if (data->core.respCode != 0) {
        digRecBuildFailure(data);
    } else if (builder) {
        if (data->core.respCode == 0) {
            builder(data);
        }
    }
    ui_receipt_update(&digitalReceipt, &header, &details, &buttons);
    LV_SHOW(digitalReceipt.root);
}

void hideDigitalRec() {
    if ((digitalReceipt.root && lv_obj_is_valid(digitalReceipt.root))) {
        LV_HIDE(digitalReceipt.root);
    }
}