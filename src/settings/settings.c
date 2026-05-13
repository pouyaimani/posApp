#include "settings.h"
#include "sys/sys.h"
#include "logger.h"
#include "common.h"
#include "utility/tlv.h"
#include "storage/storage.h"

static DevSettings __settings;

#define USER_DATA_ROOT_DIR          "/mtd0/"
#define APP_DIR                     USER_DATA_ROOT_DIR
#define DEVICE_PROP_FILE            APP_DIR"lv_device_prop"
#define SETTINGS_FILE_ADDR          DEVICE_PROP_FILE

#define SETTINGS_FILE_MAX_SIZE  4096

#define SETTINGS_FILE_HEADER_LEN    4

//default property value
#define DEFAULT_SERVER_IP                                                   ""
#define DEFAULT_SERVER_PORT                                                 "0"
#define DEFAULT_BACKUP_SERVER_IP                                            ""
#define DEFAULT_BACKUP_SERVER_PORT                                          "0"
#define DEFAULT_TMS_IP                                                      ""
#define DEFAULT_TMS_PORT                                                    "0"
#define DEFAULT_COMM_MODE                                                   "0"
#define DEFAULT_KEY_VOLUME                                                  "5"
#define DEFAULT_TIMEOUT_SLEEP                                               "5"         
#define DEFAULT_BRIGHTNESS                                                  "5"
#define DEFAULT_LANGUAGE                                                    "0"
#define DEFAULT_PRINT_GREY_SCALE                                            "1"
#define DEFAULT_UPDATE_FLAG                                                 "0"
#define DEFAULT_CONNECT_MODE                                                "1"

#define DEFAULT_TOUCH_ENABLE                                                "0"

#define DEFAULT_SHIFT_S_DATE                                                "0"
#define DEFAULT_SHIFT_S_TIME                                                "0"
#define DEFAULT_SHIFT_E_DATE                                                "0"
#define DEFAULT_SHIFT_E_TIME                                                "0"
#define DEFAULT_SHIFT_LATEST                                                "0"
#define DEFAULT_SHIFT_ENABLE                                                "0"
#define DEFAULT_SHIFT_ACTIVE                                                "0"
#define DEFAULT_MAX_AMNT_EN                                                 "0"
#define DEFAULT_MAX_AMNT                                                    "999999999999/0"
#define DEFAULT_DIRECT_SALE_EN                                              "0"
#define DEFAULT_SERVICES_EN                                                 "0"
#define DEFAULT_FIXED_AMNT_ITEM                                             "0"
#define DEFAULT_FIXED_AMNT_LIST                                             "0"
#define DEFAULT_FIXED_AMNT_COEF                                             "0"
#define DEFAULT_AMNT_LIST_CNT                                               "0"
#define DEFAULT_SSL_EN                                                      "0"
#define DEFAULT_MAIN_SERVER_ID                                              "0"
#define DEFAULT_TMS_ID                                                      "0"
#define DEFAULT_MERCHANT_PIN                                                MERCHANT_DEFAULT_PIN

BEGIN_DSC_ARRAY;
static const DataDescriptor settingsDsc[] = {

    // 🔹 Terminal - basic
    DSC_BYTE(__settings.terminal.touchEnable, DEFAULT_TOUCH_ENABLE),
    DSC_BYTE(__settings.terminal.netRoute, DEFAULT_COMM_MODE),
    DSC_BYTE(__settings.terminal.devVolume, DEFAULT_KEY_VOLUME),
    DSC_BYTE(__settings.terminal.sleepTimeout, DEFAULT_TIMEOUT_SLEEP),
    DSC_BYTE(__settings.terminal.brightness, DEFAULT_BRIGHTNESS),

    // 🔹 Login / identity
    DSC_BYTE(__settings.terminal.loginOperator, "101"),
    DSC_STR_BUF(__settings.terminal.loginDate, ""),
    DSC_STR_BUF(__settings.terminal.merchantPin, DEFAULT_MERCHANT_PIN),
    DSC_STR_BUF(__settings.terminal.merchantNo, ""),
    DSC_STR_BUF(__settings.terminal.terminalNo, ""),
    DSC_STR_BUF(__settings.terminal.merchantName, ""),

    // 🔹 WiFi
    DSC_STR_BUF(__settings.terminal.wfiSSID, ""),
    DSC_STR_BUF(__settings.terminal.wifiMac, ""),
    DSC_INT(__settings.terminal.wifiEnc, ""),
    DSC_STR_BUF(__settings.terminal.wifiPwd, ""),

    // 🔹 App
    DSC_INT(__settings.terminal.appID, ""),

    // 🔹 Main server
    DSC_STR_BUF(__settings.server.mainServerIp, DEFAULT_SERVER_IP),
    DSC_INT(__settings.server.mainServerPort, DEFAULT_SERVER_PORT),

    // 🔹 Backup server
    DSC_STR_BUF(__settings.server.backupServerIp, DEFAULT_BACKUP_SERVER_IP),
    DSC_INT(__settings.server.backupServerPort, DEFAULT_BACKUP_SERVER_PORT),

    // 🔹 Server IDs
    DSC_INT(__settings.server.mainServerId, DEFAULT_MAIN_SERVER_ID),
    DSC_INT(__settings.server.tmsId, DEFAULT_TMS_ID),

    DSC_BYTE(__settings.server.useBackupAddressFirst, "0"),

    // 🔹 TMS
    DSC_STR_BUF(__settings.server.tmsIp, DEFAULT_TMS_IP),
    DSC_INT(__settings.server.tmsPort, DEFAULT_TMS_PORT),
    DSC_STR_BUF(__settings.server.tmsBackupIp, DEFAULT_TMS_IP),
    DSC_INT(__settings.server.tmsBackupPort, DEFAULT_TMS_PORT),

    DSC_BYTE(__settings.server.sslEn, DEFAULT_SSL_EN),

    // 🔹 Terminal flags
    DSC_BYTE(__settings.terminal.language, DEFAULT_LANGUAGE),
    DSC_BYTE(__settings.terminal.printGreyScale, DEFAULT_PRINT_GREY_SCALE),
    DSC_BYTE(__settings.terminal.updateFlag, DEFAULT_UPDATE_FLAG),
    DSC_BYTE(__settings.terminal.connectMode, DEFAULT_CONNECT_MODE),

    // 🔹 Shift
    DSC_INT(__settings.terminal.shiftEnable, DEFAULT_SHIFT_ENABLE),
    DSC_INT(__settings.terminal.shiftActive, DEFAULT_SHIFT_ACTIVE),

    // 🔹 Amount settings
    DSC_INT(__settings.terminal.maxAmntEnable, DEFAULT_MAX_AMNT_EN),
    DSC_STR_BUF(__settings.terminal.maxAmnt, DEFAULT_MAX_AMNT),

    DSC_BYTE(__settings.terminal.directSaleEn, DEFAULT_DIRECT_SALE_EN),

    DSC_BIN(__settings.terminal.serviceEn, sizeof(__settings.terminal.serviceEn), sizeof(__settings.terminal.serviceEn), NULL),

    DSC_INT(__settings.terminal.fixedAmountItem, DEFAULT_FIXED_AMNT_ITEM),
    DSC_STR_BUF(__settings.terminal.amountList, DEFAULT_FIXED_AMNT_LIST),
    DSC_INT(__settings.terminal.fixedAmountCoef, DEFAULT_FIXED_AMNT_COEF),
    DSC_INT(__settings.terminal.amountListCnt, DEFAULT_AMNT_LIST_CNT),
};

static int saveSettings() {
    storage()->save(settingsDsc, sizeof(settingsDsc) / sizeof(DataDescriptor), SETTINGS_FILE_ADDR);
}

static int loadSatings() {
    storage()->load(settingsDsc, sizeof(settingsDsc) / sizeof(DataDescriptor), SETTINGS_FILE_ADDR);
    LOG_DEBUG("merchant pin = %s", __settings.terminal.merchantPin);
}

static int resetSettings() {
    storage()->reset(settingsDsc, sizeof(settingsDsc) / sizeof(DataDescriptor), SETTINGS_FILE_ADDR);
}

OOP_CTOR(DevSettings) {
    self->load = loadSatings;
    self->save = saveSettings;
    self->reset = resetSettings;
}

DevSettings *settings() {
    CALL_ONCE(
        OOP_CALL_CTOR(DevSettings, &__settings);
    );
    return &__settings;
}