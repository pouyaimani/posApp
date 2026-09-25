#include "settings.h"

#include "logger.h"
#include "storage/storage.h"

static DevSettings  __settings;
static TxnTraceInfo __txnTraceInfo;

#define USER_DATA_ROOT_DIR  "/mtd0/"
#define SETTINGS_FILE_ADDR  USER_DATA_ROOT_DIR "lv_device_prop"
#define TXN_TRACE_FILE_ADDR USER_DATA_ROOT_DIR "txn_t_info"

#define DEFAULT_SERVER_IP          ""
#define DEFAULT_SERVER_PORT        "0"
#define DEFAULT_BACKUP_SERVER_IP   ""
#define DEFAULT_BACKUP_SERVER_PORT "0"
#define DEFAULT_TMS_IP             ""
#define DEFAULT_TMS_PORT           "0"
#define DEFAULT_TMS_FORCE          "0"
#define DEFAULT_COMM_MODE          "0"
#define DEFAULT_KEY_VOLUME         "5"
#define DEFAULT_TIMEOUT_SLEEP      "5"
#define DEFAULT_TIMEOUT_POWER_OFF  "10"
#define DEFAULT_BRIGHTNESS         "5"
#define DEFAULT_LANGUAGE           "0"
#define DEFAULT_PRINT_GREY_SCALE   "1"
#define DEFAULT_UPDATE_FLAG        "0"
#define DEFAULT_CONNECT_MODE       "1"
#define DEFAULT_TOUCH_ENABLE       "0"
#define DEFAULT_CFG_DONE           "0"
#define DEFAULT_SHIFT_ENABLE       "0"
#define DEFAULT_SHIFT_ACTIVE       "0"
#define DEFAULT_MAX_AMNT_EN        "0"
#define DEFAULT_MAX_AMNT           "999999999999/0"
#define DEFAULT_DIRECT_SALE_EN     "0"
#define DEFAULT_FIXED_AMNT_ITEM    "0"
#define DEFAULT_FIXED_AMNT_COEF    "0"
#define DEFAULT_AMNT_LIST_CNT      "0"
#define DEFAULT_SSL_EN             "0"
#define DEFAULT_MAIN_SERVER_ID     "0"
#define DEFAULT_TMS_ID             "0"
#define DEFAULT_MERCHANT_PIN       MERCHANT_DEFAULT_PIN
#define DEFAULT_BALANCE_INQ_WAGE   "1800"

#define ARRAY_COUNT(a_) (sizeof(a_) / sizeof((a_)[0]))

static const DataDescriptor txnTraceInfoDsc[] = {
    DSC_U32_NAMED("stan", __txnTraceInfo.stan, "1"),
    DSC_U32_NAMED("batch", __txnTraceInfo.batch, "1"),
};

/*
 * These key strings intentionally match the old DSC_* stringized keys.
 * Never rename a persisted key. Rename only the C member if necessary.
 */
static const DataDescriptor settingsDsc[] = {
    DSC_BOOL_NAMED("touchEnable", __settings.terminal.touchEnable,
                   DEFAULT_TOUCH_ENABLE),
    DSC_U8_NAMED("isCfgDone", __settings.terminal.isCfgDone, DEFAULT_CFG_DONE),
    DSC_U8_NAMED("netRoute", __settings.terminal.netRoute, DEFAULT_COMM_MODE),
    DSC_U8_NAMED("devVolume", __settings.terminal.devVolume,
                 DEFAULT_KEY_VOLUME),
    DSC_U8_NAMED("sleepTimeout", __settings.terminal.sleepTimeout,
                 DEFAULT_TIMEOUT_SLEEP),
    DSC_U8_NAMED("powerOffTimeOut", __settings.terminal.powerOffTimeOut,
                 DEFAULT_TIMEOUT_POWER_OFF),
    DSC_U8_NAMED("brightness", __settings.terminal.brightness,
                 DEFAULT_BRIGHTNESS),
    DSC_U8_NAMED("merchantRecPrint", __settings.terminal.merchantRecPrint,
                 DEFAULT_BRIGHTNESS),
    DSC_U8_NAMED("printModel", __settings.terminal.printModel,
                 DEFAULT_BRIGHTNESS),
    DSC_U8_NAMED("autoPrint", __settings.terminal.autoPrint,
                 DEFAULT_BRIGHTNESS),
    DSC_U8_NAMED("secReceiptPrintTime", __settings.terminal.secReceiptPrintTime,
                 DEFAULT_BRIGHTNESS),

    DSC_U8_NAMED("loginOperator", __settings.terminal.loginOperator, "101"),
    DSC_STR_NAMED("loginDate", __settings.terminal.loginDate, ""),
    DSC_STR_NAMED("merchantPin", __settings.terminal.merchantPin,
                  DEFAULT_MERCHANT_PIN),
    DSC_STR_NAMED("merchantId", __settings.terminal.merchantId, ""),
    DSC_STR_NAMED("terminalId", __settings.terminal.terminalId, ""),
    DSC_STR_NAMED("merchantName", __settings.terminal.merchantName, ""),
    DSC_STR_NAMED("merchantAddress", __settings.terminal.merchantAddress, ""),
    DSC_STR_NAMED("merchantPostalCode", __settings.terminal.merchantPostalCode,
                  ""),
    DSC_STR_NAMED("merchantUniqueId", __settings.terminal.merchantUniqueId, ""),
    DSC_STR_NAMED("merchantPhone", __settings.terminal.merchantPhone, ""),
    DSC_STR_NAMED("mSafePwd", __settings.terminal.mSafePwd, ""),

    DSC_STR_NAMED("wfiSSID", __settings.terminal.wfiSSID, ""),
    DSC_STR_NAMED("wifiMac", __settings.terminal.wifiMac, ""),
    DSC_U32_NAMED("wifiEnc", __settings.terminal.wifiEnc, "0"),
    DSC_STR_NAMED("wifiPwd", __settings.terminal.wifiPwd, ""),
    DSC_STR_NAMED("acquirerIIN", __settings.terminal.acquirerIIN, ""),
    DSC_U32_NAMED("appID", __settings.terminal.appID, "0"),

    DSC_STR_NAMED("mainServerIp", __settings.server.mainServerIp,
                  DEFAULT_SERVER_IP),
    DSC_U16_NAMED("mainServerPort", __settings.server.mainServerPort,
                  DEFAULT_SERVER_PORT),
    DSC_STR_NAMED("backupServerIp", __settings.server.backupServerIp,
                  DEFAULT_BACKUP_SERVER_IP),
    DSC_U16_NAMED("backupServerPort", __settings.server.backupServerPort,
                  DEFAULT_BACKUP_SERVER_PORT),
    DSC_U16_NAMED("mainServerNii", __settings.server.mainServerNii,
                  DEFAULT_MAIN_SERVER_ID),
    DSC_U16_NAMED("tmsId", __settings.server.tmsId, DEFAULT_TMS_ID),
    DSC_BOOL_NAMED("useBackupAddressFirst",
                   __settings.server.useBackupAddressFirst, "0"),

    DSC_STR_NAMED("tmsIp", __settings.server.tmsIp, DEFAULT_TMS_IP),
    DSC_U16_NAMED("tmsPort", __settings.server.tmsPort, DEFAULT_TMS_PORT),
    DSC_STR_NAMED("tmsBackupIp", __settings.server.tmsBackupIp, DEFAULT_TMS_IP),
    DSC_U16_NAMED("tmsBackupPort", __settings.server.tmsBackupPort,
                  DEFAULT_TMS_PORT),
    DSC_U8_NAMED("forceTMS", __settings.server.forceTMS, DEFAULT_TMS_FORCE),
    DSC_BOOL_NAMED("sslEn", __settings.server.sslEn, DEFAULT_SSL_EN),

    DSC_U8_NAMED("language", __settings.terminal.language, DEFAULT_LANGUAGE),
    DSC_U8_NAMED("printGreyScale", __settings.terminal.printGreyScale,
                 DEFAULT_PRINT_GREY_SCALE),
    DSC_U8_NAMED("updateFlag", __settings.terminal.updateFlag,
                 DEFAULT_UPDATE_FLAG),
    DSC_U8_NAMED("connectMode", __settings.terminal.connectMode,
                 DEFAULT_CONNECT_MODE),
    DSC_U8_NAMED("shiftEnable", __settings.terminal.shiftEnable,
                 DEFAULT_SHIFT_ENABLE),
    DSC_U8_NAMED("shiftActive", __settings.terminal.shiftActive,
                 DEFAULT_SHIFT_ACTIVE),

    DSC_BOOL_NAMED("maxAmntEnable", __settings.terminal.maxAmntEnable,
                   DEFAULT_MAX_AMNT_EN),
    DSC_STR_NAMED("maxAmnt", __settings.terminal.maxAmnt, DEFAULT_MAX_AMNT),
    DSC_BOOL_NAMED("directSaleEn", __settings.terminal.directSaleEn,
                   DEFAULT_DIRECT_SALE_EN),
    DSC_BIN_NAMED("chItemStatus", __settings.terminal.chItemStatus),
    DSC_U32_NAMED("fixedAmountItem", __settings.terminal.fixedAmountItem,
                  DEFAULT_FIXED_AMNT_ITEM),
    DSC_BIN_NAMED("amountList", __settings.terminal.amountList),
    DSC_U32_NAMED("fixedAmountCoef", __settings.terminal.fixedAmountCoef,
                  DEFAULT_FIXED_AMNT_COEF),
    DSC_U32_NAMED("amountListCnt", __settings.terminal.amountListCnt,
                  DEFAULT_AMNT_LIST_CNT),

    DSC_U64_NAMED("balanceInqWage", __settings.txn.balanceInqWage,
                  DEFAULT_BALANCE_INQ_WAGE),
};

static Error_t saveSettings(void) {
    return storage()->save(settingsDsc, ARRAY_COUNT(settingsDsc),
                           SETTINGS_FILE_ADDR);
}

static Error_t loadSettings(void) {
    return storage()->load(settingsDsc, ARRAY_COUNT(settingsDsc),
                           SETTINGS_FILE_ADDR);
}

static Error_t resetSettings(void) {
    return storage()->reset(settingsDsc, ARRAY_COUNT(settingsDsc),
                            SETTINGS_FILE_ADDR);
}

OOP_CTOR(DevSettings) {
    self->load  = loadSettings;
    self->save  = saveSettings;
    self->reset = resetSettings;
}

DevSettings* settings(void) {
    CALL_ONCE(OOP_CALL_CTOR(DevSettings, &__settings););
    return &__settings;
}

static Error_t loadTxnTraceInfo(void) {
    return storage()->load(txnTraceInfoDsc, ARRAY_COUNT(txnTraceInfoDsc),
                           TXN_TRACE_FILE_ADDR);
}

static Error_t incTxnTraceInfo(void) {
    Error_t err = loadTxnTraceInfo();
    if (err != ERR_OK)
        return err;

    __txnTraceInfo.stan++;
    if (__txnTraceInfo.stan >= 999999u) {
        __txnTraceInfo.stan = 1u;
        __txnTraceInfo.batch++;
        if (__txnTraceInfo.batch >= 999999u)
            __txnTraceInfo.batch = 1u;
    }

    return storage()->save(txnTraceInfoDsc, ARRAY_COUNT(txnTraceInfoDsc),
                           TXN_TRACE_FILE_ADDR);
}

OOP_CTOR(TxnTraceInfo) {
    self->load = loadTxnTraceInfo;
    self->inc  = incTxnTraceInfo;
}

TxnTraceInfo* txnTraceInfo(void) {
    CALL_ONCE(OOP_CALL_CTOR(TxnTraceInfo, &__txnTraceInfo);
              (void)loadTxnTraceInfo(););
    return &__txnTraceInfo;
}
