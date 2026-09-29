#include "settings.h"

#include "logger.h"
#include "storage/storage.h"

static DevSettings  __settings;
static TxnTraceInfo __txnTraceInfo;

#define USER_DATA_ROOT_DIR  "/mtd0/"
#define SETTINGS_FILE_ADDR  USER_DATA_ROOT_DIR "settings.bin"
#define TXN_TRACE_FILE_ADDR USER_DATA_ROOT_DIR "txn_t_info.bin"

#define DEFAULT_SERVER_IP            ""
#define DEFAULT_SERVER_PORT          "0"
#define DEFAULT_BACKUP_SERVER_IP     ""
#define DEFAULT_BACKUP_SERVER_PORT   "0"
#define DEFAULT_TMS_IP               ""
#define DEFAULT_TMS_PORT             "0"
#define DEFAULT_TMS_FORCE            "0"
#define DEFAULT_COMM_MODE            "0"
#define DEFAULT_KEY_VOLUME           "5"
#define DEFAULT_TIMEOUT_SLEEP        "5"
#define DEFAULT_TIMEOUT_POWER_OFF    "10"
#define DEFAULT_BRIGHTNESS           "5"
#define DEFAULT_MERCHANT_REC_PRINT   "5"
#define DEFAULT_PRINT_MODEL          "5"
#define DEFAULT_AUTO_PRINT           "5"
#define DEFAULT_SECOND_RECEIPT_TIME  "5"
#define DEFAULT_LANGUAGE             "0"
#define DEFAULT_PRINT_GREY_SCALE     "1"
#define DEFAULT_UPDATE_FLAG          "0"
#define DEFAULT_CONNECT_MODE         "1"
#define DEFAULT_TOUCH_ENABLE         "0"
#define DEFAULT_CFG_DONE             "0"
#define DEFAULT_SHIFT_ENABLE         "0"
#define DEFAULT_SHIFT_ACTIVE         "0"
#define DEFAULT_MAX_AMNT_EN          "0"
#define DEFAULT_MAX_AMNT             "999999999999/0"
#define DEFAULT_DIRECT_SALE_EN       "0"
#define DEFAULT_FIXED_AMNT_ITEM      "0"
#define DEFAULT_FIXED_AMNT_COEF      "0"
#define DEFAULT_AMNT_LIST_CNT        "0"
#define DEFAULT_SSL_EN               "0"
#define DEFAULT_MAIN_SERVER_ID       "0"
#define DEFAULT_TMS_ID               "0"
#define DEFAULT_MERCHANT_PIN         MERCHANT_DEFAULT_PIN
#define DEFAULT_BALANCE_INQUIRY_WAGE "1800"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static const DataDescriptor txnTraceInfoDsc[] = {
    DSC_U32_ID(SETTING_TAG_TRACE_STAN, __txnTraceInfo.stan, "1"),
    DSC_U32_ID(SETTING_TAG_TRACE_BATCH, __txnTraceInfo.batch, "1"),
};

static const DataDescriptor settingsDsc[] = {
    DSC_BOOL_ID(SETTING_TAG_TOUCH_ENABLE, __settings.terminal.touchEnable,
                DEFAULT_TOUCH_ENABLE),
    DSC_U8_ID(SETTING_TAG_CFG_DONE, __settings.terminal.isCfgDone,
              DEFAULT_CFG_DONE),
    DSC_U8_ID(SETTING_TAG_NET_ROUTE, __settings.terminal.netRoute,
              DEFAULT_COMM_MODE),
    DSC_U8_ID(SETTING_TAG_DEVICE_VOLUME, __settings.terminal.devVolume,
              DEFAULT_KEY_VOLUME),
    DSC_U8_ID(SETTING_TAG_SLEEP_TIMEOUT, __settings.terminal.sleepTimeout,
              DEFAULT_TIMEOUT_SLEEP),
    DSC_U8_ID(SETTING_TAG_POWER_OFF_TIMEOUT,
              __settings.terminal.powerOffTimeOut, DEFAULT_TIMEOUT_POWER_OFF),
    DSC_U8_ID(SETTING_TAG_BRIGHTNESS, __settings.terminal.brightness,
              DEFAULT_BRIGHTNESS),

    DSC_U8_ID(SETTING_TAG_MERCHANT_REC_PRINT,
              __settings.terminal.merchantRecPrint, DEFAULT_MERCHANT_REC_PRINT),
    DSC_U8_ID(SETTING_TAG_PRINT_MODEL, __settings.terminal.printModel,
              DEFAULT_PRINT_MODEL),
    DSC_U8_ID(SETTING_TAG_AUTO_PRINT, __settings.terminal.autoPrint,
              DEFAULT_AUTO_PRINT),
    DSC_U8_ID(SETTING_TAG_PRINT_GREY_SCALE, __settings.terminal.printGreyScale,
              DEFAULT_PRINT_GREY_SCALE),
    DSC_U8_ID(SETTING_TAG_SECOND_RECEIPT_TIME,
              __settings.terminal.secReceiptPrintTime,
              DEFAULT_SECOND_RECEIPT_TIME),

    DSC_U8_ID(SETTING_TAG_LOGIN_OPERATOR, __settings.terminal.loginOperator,
              "101"),
    DSC_STR_ID(SETTING_TAG_LOGIN_DATE, __settings.terminal.loginDate, ""),
    DSC_STR_ID(SETTING_TAG_MERCHANT_PIN, __settings.terminal.merchantPin,
               DEFAULT_MERCHANT_PIN),
    DSC_STR_ID(SETTING_TAG_MERCHANT_ID, __settings.terminal.merchantId, ""),
    DSC_STR_ID(SETTING_TAG_TERMINAL_ID, __settings.terminal.terminalId, ""),
    DSC_STR_ID(SETTING_TAG_MERCHANT_NAME, __settings.terminal.merchantName, ""),
    DSC_STR_ID(SETTING_TAG_MERCHANT_ADDRESS,
               __settings.terminal.merchantAddress, ""),
    DSC_STR_ID(SETTING_TAG_MERCHANT_POSTAL_CODE,
               __settings.terminal.merchantPostalCode, ""),
    DSC_STR_ID(SETTING_TAG_MERCHANT_UNIQUE_ID,
               __settings.terminal.merchantUniqueId, ""),
    DSC_STR_ID(SETTING_TAG_MERCHANT_PHONE, __settings.terminal.merchantPhone,
               ""),
    DSC_STR_ID(SETTING_TAG_MSAFE_PASSWORD, __settings.terminal.mSafePwd, ""),

    DSC_STR_ID(SETTING_TAG_WIFI_SSID, __settings.terminal.wfiSSID, ""),
    DSC_STR_ID(SETTING_TAG_WIFI_MAC, __settings.terminal.wifiMac, ""),
    DSC_U32_ID(SETTING_TAG_WIFI_ENCRYPTION, __settings.terminal.wifiEnc, "0"),
    DSC_STR_ID(SETTING_TAG_WIFI_PASSWORD, __settings.terminal.wifiPwd, ""),
    DSC_STR_ID(SETTING_TAG_ACQUIRER_IIN, __settings.terminal.acquirerIIN, ""),
    DSC_U32_ID(SETTING_TAG_APP_ID, __settings.terminal.appID, "0"),

    DSC_STR_ID(SETTING_TAG_MAIN_SERVER_IP, __settings.server.mainServerIp,
               DEFAULT_SERVER_IP),
    DSC_U16_ID(SETTING_TAG_MAIN_SERVER_PORT, __settings.server.mainServerPort,
               DEFAULT_SERVER_PORT),
    DSC_STR_ID(SETTING_TAG_BACKUP_SERVER_IP, __settings.server.backupServerIp,
               DEFAULT_BACKUP_SERVER_IP),
    DSC_U16_ID(SETTING_TAG_BACKUP_SERVER_PORT,
               __settings.server.backupServerPort, DEFAULT_BACKUP_SERVER_PORT),
    DSC_U16_ID(SETTING_TAG_MAIN_SERVER_NII, __settings.server.mainServerNii,
               DEFAULT_MAIN_SERVER_ID),
    DSC_U16_ID(SETTING_TAG_TMS_ID, __settings.server.tmsId, DEFAULT_TMS_ID),
    DSC_BOOL_ID(SETTING_TAG_USE_BACKUP_FIRST,
                __settings.server.useBackupAddressFirst, "0"),

    DSC_STR_ID(SETTING_TAG_TMS_IP, __settings.server.tmsIp, DEFAULT_TMS_IP),
    DSC_U16_ID(SETTING_TAG_TMS_PORT, __settings.server.tmsPort,
               DEFAULT_TMS_PORT),
    DSC_STR_ID(SETTING_TAG_TMS_BACKUP_IP, __settings.server.tmsBackupIp,
               DEFAULT_TMS_IP),
    DSC_U16_ID(SETTING_TAG_TMS_BACKUP_PORT, __settings.server.tmsBackupPort,
               DEFAULT_TMS_PORT),
    DSC_U8_ID(SETTING_TAG_FORCE_TMS, __settings.server.forceTMS,
              DEFAULT_TMS_FORCE),
    DSC_BOOL_ID(SETTING_TAG_SSL_ENABLE, __settings.server.sslEn,
                DEFAULT_SSL_EN),

    DSC_U8_ID(SETTING_TAG_LANGUAGE, __settings.terminal.language,
              DEFAULT_LANGUAGE),
    DSC_U8_ID(SETTING_TAG_UPDATE_FLAG, __settings.terminal.updateFlag,
              DEFAULT_UPDATE_FLAG),
    DSC_U8_ID(SETTING_TAG_CONNECT_MODE, __settings.terminal.connectMode,
              DEFAULT_CONNECT_MODE),
    DSC_U8_ID(SETTING_TAG_SHIFT_ENABLE, __settings.terminal.shiftEnable,
              DEFAULT_SHIFT_ENABLE),
    DSC_U8_ID(SETTING_TAG_SHIFT_ACTIVE, __settings.terminal.shiftActive,
              DEFAULT_SHIFT_ACTIVE),

    DSC_BOOL_ID(SETTING_TAG_MAX_AMOUNT_ENABLE,
                __settings.terminal.maxAmntEnable, DEFAULT_MAX_AMNT_EN),
    DSC_STR_ID(SETTING_TAG_MAX_AMOUNT, __settings.terminal.maxAmnt,
               DEFAULT_MAX_AMNT),
    DSC_BOOL_ID(SETTING_TAG_DIRECT_SALE_ENABLE,
                __settings.terminal.directSaleEn, DEFAULT_DIRECT_SALE_EN),
    DSC_BIN_ID(SETTING_TAG_CARDHOLDER_ITEMS, __settings.terminal.chItemStatus),
    DSC_U32_ID(SETTING_TAG_FIXED_AMOUNT_ITEM,
               __settings.terminal.fixedAmountItem, DEFAULT_FIXED_AMNT_ITEM),
    DSC_BIN_ID(SETTING_TAG_AMOUNT_LIST, __settings.terminal.amountList),
    DSC_U32_ID(SETTING_TAG_FIXED_AMOUNT_COEF,
               __settings.terminal.fixedAmountCoef, DEFAULT_FIXED_AMNT_COEF),
    DSC_U32_ID(SETTING_TAG_AMOUNT_LIST_COUNT, __settings.terminal.amountListCnt,
               DEFAULT_AMNT_LIST_CNT),

    DSC_U64_ID(SETTING_TAG_BALANCE_INQUIRY_WAGE, __settings.txn.balanceInqWage,
               DEFAULT_BALANCE_INQUIRY_WAGE),
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
    Error_t result = loadTxnTraceInfo();

    if (result != ERR_OK)
        return result;

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
