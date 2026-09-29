#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <stdbool.h>
#include <stdint.h>

#include "common.h"
#include "len.h"
#include "oop.h"

/*
 * Persistent IDs are permanent. Never reuse an ID, even after removing a
 * setting. Explicit assignments allow descriptors and members to be reordered.
 */
typedef enum {
    /* Terminal core: 0x0100-0x011F */
    SETTING_TAG_TOUCH_ENABLE        = 0x0101,
    SETTING_TAG_CFG_DONE            = 0x0102,
    SETTING_TAG_NET_ROUTE           = 0x0103,
    SETTING_TAG_DEVICE_VOLUME       = 0x0104,
    SETTING_TAG_SLEEP_TIMEOUT       = 0x0105,
    SETTING_TAG_POWER_OFF_TIMEOUT   = 0x0106,
    SETTING_TAG_BRIGHTNESS          = 0x0107,
    SETTING_TAG_MERCHANT_REC_PRINT  = 0x0108,
    SETTING_TAG_PRINT_MODEL         = 0x0109,
    SETTING_TAG_AUTO_PRINT          = 0x010A,
    SETTING_TAG_SECOND_RECEIPT_TIME = 0x010B,

    /* Login and merchant: 0x0120-0x013F */
    SETTING_TAG_LOGIN_OPERATOR       = 0x0120,
    SETTING_TAG_LOGIN_DATE           = 0x0121,
    SETTING_TAG_MERCHANT_PIN         = 0x0122,
    SETTING_TAG_MERCHANT_ID          = 0x0123,
    SETTING_TAG_TERMINAL_ID          = 0x0124,
    SETTING_TAG_MERCHANT_NAME        = 0x0125,
    SETTING_TAG_MERCHANT_ADDRESS     = 0x0126,
    SETTING_TAG_MERCHANT_POSTAL_CODE = 0x0127,
    SETTING_TAG_MERCHANT_UNIQUE_ID   = 0x0128,
    SETTING_TAG_MERCHANT_PHONE       = 0x0129,
    SETTING_TAG_MSAFE_PASSWORD       = 0x012A,

    /* Wi-Fi and application: 0x0140-0x015F */
    SETTING_TAG_WIFI_SSID       = 0x0140,
    SETTING_TAG_WIFI_MAC        = 0x0141,
    SETTING_TAG_WIFI_ENCRYPTION = 0x0142,
    SETTING_TAG_WIFI_PASSWORD   = 0x0143,
    SETTING_TAG_ACQUIRER_IIN    = 0x0144,
    SETTING_TAG_APP_ID          = 0x0145,

    /* General terminal behavior: 0x0160-0x017F */
    SETTING_TAG_LANGUAGE         = 0x0160,
    SETTING_TAG_PRINT_GREY_SCALE = 0x0161,
    SETTING_TAG_UPDATE_FLAG      = 0x0162,
    SETTING_TAG_CONNECT_MODE     = 0x0163,
    SETTING_TAG_SHIFT_ENABLE     = 0x0164,
    SETTING_TAG_SHIFT_ACTIVE     = 0x0165,

    /* Amount behavior: 0x0180-0x019F */
    SETTING_TAG_MAX_AMOUNT_ENABLE  = 0x0180,
    SETTING_TAG_MAX_AMOUNT         = 0x0181,
    SETTING_TAG_DIRECT_SALE_ENABLE = 0x0182,
    SETTING_TAG_CARDHOLDER_ITEMS   = 0x0183,
    SETTING_TAG_FIXED_AMOUNT_ITEM  = 0x0184,
    SETTING_TAG_AMOUNT_LIST        = 0x0185,
    SETTING_TAG_FIXED_AMOUNT_COEF  = 0x0186,
    SETTING_TAG_AMOUNT_LIST_COUNT  = 0x0187,

    /* Server and TMS: 0x0200-0x02FF */
    SETTING_TAG_MAIN_SERVER_IP     = 0x0201,
    SETTING_TAG_MAIN_SERVER_PORT   = 0x0202,
    SETTING_TAG_BACKUP_SERVER_IP   = 0x0203,
    SETTING_TAG_BACKUP_SERVER_PORT = 0x0204,
    SETTING_TAG_MAIN_SERVER_NII    = 0x0205,
    SETTING_TAG_TMS_ID             = 0x0206,
    SETTING_TAG_USE_BACKUP_FIRST   = 0x0207,
    SETTING_TAG_TMS_IP             = 0x0220,
    SETTING_TAG_TMS_PORT           = 0x0221,
    SETTING_TAG_TMS_BACKUP_IP      = 0x0222,
    SETTING_TAG_TMS_BACKUP_PORT    = 0x0223,
    SETTING_TAG_FORCE_TMS          = 0x0224,
    SETTING_TAG_SSL_ENABLE         = 0x0225,

    /* Transaction settings: 0x0300-0x03FF */
    SETTING_TAG_BALANCE_INQUIRY_WAGE = 0x0301,

    /* Transaction trace file: 0x0400-0x04FF */
    SETTING_TAG_TRACE_STAN  = 0x0401,
    SETTING_TAG_TRACE_BATCH = 0x0402,

    SETTING_TAG_INVALID = 0xFFFF
} SettingsTag;

typedef struct {
    bool    touchEnable;
    uint8_t netRoute;
    uint8_t devVolume;
    uint8_t sleepTimeout;
    uint8_t powerOffTimeOut;
    uint8_t brightness;
    uint8_t shiftEnable;
    uint8_t shiftActive;
    uint8_t isCfgDone;
    uint8_t language;
    uint8_t updateFlag;
    uint8_t connectMode;

    uint8_t merchantRecPrint;
    uint8_t printModel;
    uint8_t autoPrint;
    uint8_t printGreyScale;
    uint8_t secReceiptPrintTime;

    uint8_t  loginOperator;
    char     loginDate[6 + 1];
    char     merchantPin[4 + 1];
    char     merchantId[15 + 1];
    char     merchantAddress[64];
    char     merchantPostalCode[16];
    char     merchantUniqueId[32];
    char     merchantName[40 + 1];
    char     merchantPhone[32];
    char     terminalId[15 + 1];
    char     mSafePwd[8 + 1];
    char     wfiSSID[32];
    char     wifiMac[32];
    uint32_t wifiEnc;
    char     wifiPwd[63 + 1];
    char     acquirerIIN[12];
    uint32_t appID;
    bool     maxAmntEnable;
    char     maxAmnt[14 + 1];
    bool     directSaleEn;
    bool     chItemStatus[LEN_MAX_CARDHOLDER_ITEMS];
    int32_t  fixedAmountItem;
    char     amountList[LEN_MAX_AMOUNT_LIST][12 + 1];
    int32_t  amountListCnt;
    int32_t  fixedAmountCoef;
} TerminalSettings;

typedef struct {
    char     mainServerIp[15 + 1];
    uint16_t mainServerPort;
    uint16_t mainServerNii;
    char     backupServerIp[15 + 1];
    uint16_t backupServerPort;
    bool     useBackupAddressFirst;
    char     tmsIp[15 + 1];
    uint16_t tmsPort;
    uint16_t tmsId;
    char     tmsBackupIp[15 + 1];
    uint16_t tmsBackupPort;
    bool     sslEn;
    uint8_t  forceTMS;
} ServerSettings;

OOP_CLASS(TxnSettings) { uint64_t balanceInqWage; };

/* These are in-memory objects; persistence is descriptor-based, not raw. */
typedef struct {
    OOP_METHOD(Error_t, save);
    OOP_METHOD(Error_t, load);
    OOP_METHOD(Error_t, reset);
    TerminalSettings terminal;
    ServerSettings   server;
    TxnSettings      txn;
} DevSettings;

DevSettings* settings(void);

typedef struct {
    OOP_METHOD(Error_t, inc);
    OOP_METHOD(Error_t, load);
    uint32_t stan;
    uint32_t batch;
} TxnTraceInfo;

TxnTraceInfo* txnTraceInfo(void);

#endif /* SETTINGS_H_ */
