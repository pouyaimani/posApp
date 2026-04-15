#include "settings.h"
#include "dev/dev.h"
#include "logger.h"
#include "common.h"
#include "utility/tlv.h"
#include "storage/storage.h"

#define DSC_BYTE(prop, def, field) \
    { (prop), T_BYTE, (0), (sizeof(field)), (def), &(field) }

#define DSC_INT(prop, def, field) \
    { (prop), T_INT, (0), (sizeof(field)), (def), &(field) }

#define DSC_STR_BUF(prop, field, def) \
    { (prop), T_STRING, (0), sizeof(field) - 1, (def), (field) }

#define DSC_BIN(prop, min, max, def, field) \
    { (prop), T_BINARY, (min), (max), (def), (void*)(field) }


#define APP_DIR                     USER_DATA_ROOT_DIR
#define DEVICE_PROP_FILE            APP_DIR"lv_device_prop"

#define SETTINGS_FILE_ADDR          DEVICE_PROP_FILE

static DevSettings __settings;

#define DEFAULT_SERVER_IP               ""
#define DEFAULT_SERVER_PORT             "0"
#define DEFAULT_BACKUP_SERVER_IP               ""
#define DEFAULT_BACKUP_SERVER_PORT             "0"
#define DEFAULT_TMS_IP               ""
#define DEFAULT_TMS_PORT             "0"

#define USER_DATA_ROOT_DIR          "/mtd0/"

#define APP_DIR                     USER_DATA_ROOT_DIR
#define DEVICE_PROP_FILE            APP_DIR"lv_device_prop"

#define SETTINGS_FILE_MAX_SIZE  4096

#define SETTINGS_FILE_HEADER_LEN    4

#define PROP_LANGUAGE                           "language"
#define PROP_PRINT_GREY_SCALE                   "print_grey_scale"
#define PROP_UPDATE_FLAG                        "update_flag"
#define PROP_CONNECT_MODE                       "connect_mode"
#define PROP_IS_OPEN_KEY_VOLUME                 "is_open_key_volume"
#define PROP_KEY_VOLUME                         "key_volume"
#define PROP_HAS_MARGIN                         "has_margin"
#define PROP_HAS_DOWNLOAD_TMK                   "has_download_tmk"
#define PROP_HAS_VERSION_FORCE_UPDATE           "has_version_force_update"
#define PROP_TOUCH_ENABLE                       "touch_enable"
#define PROP_COMM_MODE                          "comm_mode"
#define PROP_TIMEOUT_SLEEP                      "timeout_sleep"
#define PROP_BRIGHTNESS                         "brightness"
#define PROP_LOGIN_OPERATOR                     "login_operator"
#define PROP_HAS_LOGIN_ONLINE                   "has_login_online"
#define PROP_LOGIN_DATE                         "login_date"
#define PROP_MERCHANT_PIN                       "operator_pwd"
#define PROP_99_PWD                             "pwd_99"
#define PROP_00_PWD                             "pwd_00"
#define PROP_SAFE_PWD                           "safe_pwd"
#define PROP_MERCHANT_NO                        "merchant_no"
#define PROP_TERMINAL_NO                        "terminal_no"
#define PROP_MERCHANT_NAME                      "merchant_name"
#define PROP_BATCH_NO                           "batch_no"
#define PROP_VOUCHER_NO                         "voucher_no"
#define PROP_CREATE_LINK_ATTR                   "create_link_attr"
#define PROP_WIFI_SSID                          "wifi_ssid"
#define PROP_WIFI_MAC                           "wifi_mac"
#define PROP_WIFI_ENC                           "wifi_enc"
#define PROP_WIFI_PWD                           "wifi_pwd"
#define PROP_TMS_CHECK_DATE                     "tms_check_date"
#define PROP_TIMEOUT_SHOW_ERR                   "timeout_show_err"
#define PROP_TIMEOUT_SHOW_TIP                   "timeout_show_tip"
#define PROP_CREATE_PPP_TIMEOUT                 "create_ppp_timeout"
#define PROP_TRANSMIT_ERR_TIPS_TIMEOUT          "err_transmit_tips_timeout"
#define PROP_RECV_DATA_TIMEOUT                  "recv_data_timeout"
#define PROP_APP_ID                             "app_id"

#define PROP_MAIN_SERVER_IP                     "main_server_ip"
#define PROP_MAIN_SERVER_PORT                   "main_port"
#define PROP_BACKUP_SERVER_IP                   "backup_server_ip"
#define PROP_BACKUP_SERVER_PORT                 "backup_server_port"
#define PROP_USE_BACKUP_FIRST                   "use_backup_first"
#define PROP_TMS_IP                             "tms_ip"
#define PROP_TMS_PORT                           "tms_port"
#define PROP_TMS_BACKUP_IP                      "tms_backup_ip"
#define PROP_TMS_BACKUP_PORT                    "tms_backup_port"
#define PROP_SSL_EN                             "en_ssl"
#define PROP_MAIN_SERVER_ID                     "main_server_id"
#define PROP_TMS_ID                             "tms_id"

#define PROP_IS_RF_FIRST                        "is_rf_first"
#define PROP_IS_SUPPORT_QPS                     "is_support_qps"
#define PROP_IS_SUPPORT_NO_SIGN                 "is_support_no_sign"
#define PROP_QPS_NO_PIN_AMOUNT                  "qps_no_pin_amount"
#define PROP_QPS_NO_SIGN_AMOUNT                 "qps_no_sign_amount"
#define PROP_IS_QPS_BINA                        "is_qps_bin_a"
#define PROP_IS_QPS_BINB                        "is_qps_bin_b"
#define PROP_IS_QPS_CDCVM                       "is_qps_bin_cdcvm"
#define PROP_IS_SUPPORT_MAG_12_PWD              "is_support_mag_12_pwd"
#define PROP_IS_SUPPORT_EMPTY_PWD               "is_support_empty_pwd"
#define PROP_IS_WK_3DES                         "is_wk_3des"
#define PROP_IS_SUPPORT_TDK                     "is_support_tdk"
#define PROP_IS_AUTO_LOGOUT                     "is_auto_logout"
#define PROP_ICC_AID_COUNT                      "icc_aid_count"
#define PROP_ICC_CAPK_COUNT                     "icc_capk_count"
#define PROP_TMK_INDEX                          "tmk_index"
#define PROP_WK_INDEX                           "wk_index"
#define PROP_REVERSAL_TIMES                     "reversal_times"
#define PROP_TPDU                               "tpdu"
#define PROP_IS_PRINT_DETAIL                    "is_print_detail"
#define PROP_PRINT_TIMES                        "print_times"
#define PROP_PRINT_TIMES_QRCODE                 "print_times_qrcode"
#define PROP_IS_SUPPORT_GM                      "is_support_gm"
#define PROP_IS_VERIFY_MAC                      "is_verify_mac"

#define PROP_SUPPORT_AUTO_LOGIN                 "support_auto_login"
#define PROP_SUPPORT_REVERSE                    "support_reverse"
#define PROP_SUPPORT_ES_BEFORE_ONLINE           "support_es_before_online"

#define PROP_SHIFT_DATA                           "shifts_data"
#define PROP_SHIFT_LATEST                         "shift_latest"
#define PROP_SHIFT_ENABLE                         "shift_enable"
#define PROP_SHIFT_ACTIVE                         "shift_active"

#define PROP_MAX_AMNT_EN                          "max_amnt_en"
#define PROP_MAX_AMNT                             "max_amnt"

#define PROP_DIRECT_SALE_EN                       "direct_sale_en"

#define PROP_SERVICES_EN                          "services_en"

#define PROP_FIXED_AMNT_ITEM                      "fixed_amnt_item"
#define PROP_FIXED_AMNT_LIST                      "fixed_amnt_list"
#define PROP_FIXED_AMNT_COEF                      "fixed_amnt_coef"
#define PROP_AMNT_LIST_CNT                        "amnt_list_cnt"

//default property value
#define DEFAULT_PROP_IS_OPEN_KEY_VOLUME        "1"                         ///<按键音开关
#define DEFAULT_PROP_HAS_MARGIN                "0"                         ///<押金缴纳标识
#define DEFAULT_PROP_HAS_DOWNLOAD_TMK          "0"                         ///<主密钥下载标识
#define DEFAULT_PROP_HAS_VERSION_FORCE_UPDATE  "0"                         ///<TMS强制更新标识
#define DEFAULT_PROP_COMM_MODE                 "0"                         ///<通讯方式
#define DEFAULT_PROP_KEY_VOLUME                "5"                         ///<按键音量
#define DEFAULT_PROP_TIMEOUT_SLEEP             "5"                        ///<休眠超时时间
#define DEFAULT_PROP_TIME_CLOSE_LIGHT_BEFORE_SLEEP      "30"               ///<休眠前关闭背光的时间
#define DEFAULT_PROP_BRIGHTNESS                "5"                         ///<亮度
#define DEFAULT_PROP_OPERATOR_PWD              "10000|10000|10000|10000"   ///<操作员属性,第一位:是否存在,后四位:密码
#define DEFAULT_PROP_99_PWD                    "12345678"                  ///<99密码
#define DEFAULT_PROP_00_PWD                    "123456"                    ///<00密码
#define DEFAULT_PROP_SAFE_PWD                  "12345678"                  ///<安全密码
#define DEFAULT_PROP_CREATE_LINK_ATTR          "4:32:50:50:50"             ///<创建连接超时属性,第一个字段:重试次数,后续字段:每次超时时间
#define DEFAULT_PROP_TIMEOUT_SHOW_ERR          "15"                     ///<错误提示时间
#define DEFAULT_PROP_TIMEOUT_SHOW_TIP          "30"                     ///<信息提示时间
#define DEFAULT_PROP_CREATE_PPP_TIMEOUT        "60"                     ///<注册网络超时时间
#define DEFAULT_PROP_TRANSMIT_ERR_TIPS_TIMEOUT "30"                     ///<数据传输错误提示时间
#define DEFAULT_PROP_RECV_DATA_TIMEOUT         "60"                     ///<接收数据超时时间
#define DEFAULT_PROP_IS_SUPPORT_QPS            "1"                         ///<免密标识
#define DEFAULT_PROP_IS_SUPPORT_NO_SIGN        "1"                         ///<免签标识
#define DEFAULT_PROP_IS_SUPPORT_MAG_12_PWD     "1"                         ///<12位磁卡密码标识
#define DEFAULT_PROP_IS_SUPPORT_EMPTY_PWD      "1"                         ///<允许跳过密码标识
#define DEFAULT_PROP_IS_QPS_BINA               "1"                         ///<卡BINA标识
#define DEFAULT_PROP_IS_QPS_BINB               "1"                         ///<卡BINB标识
#define DEFAULT_PROP_IS_QPS_CDCVM              "1"                         ///<CDCVM标识
#define DEFAULT_PROP_IS_RF_FIRST               "0"                         ///<挥卡优先标识
#define DEFAULT_PROP_IS_WK_3DES                "0"                         ///<3DES标识和TDK共用默认的
#define DEFAULT_PROP_IS_AUTO_LOGOUT            "1"                         ///<结算后自动签退标识
#define DEFAULT_PROP_IS_PRINT_DETAIL           "1"                         ///<打印明细标识
#define DEFAULT_PROP_PRINT_TIMES               "2"                         ///<打印小票联数
#define DEFAULT_PROP_REVERSAL_TIMES            "3"                         ///<冲正次数
#define DEFAULT_PROP_TPDU                      "6000060000"                ///<TPDU
#define DEFAULT_PROP_QPS_NO_PIN_AMOUNT         "000000100000"              ///<免密限额
#define DEFAULT_PROP_QPS_NO_SIGN_AMOUNT        "000000100000"              ///<免签限额
#define DEFAULT_PROP_IS_SUPPORT_GM             "0"                         ///<国密标识
#define DEFAULT_PROP_IS_VERIFY_MAC             "1"                         ///<校验MAC标识
#define DEFAULT_PROP_LANGUAGE                  "0"
#define DEFAULT_PROP_PRINT_GREY_SCALE          "1"
#define DEFAULT_PROP_UPDATE_FLAG               "0"
#define DEFAULT_PROP_CONNECT_MODE              "1"

#define DEFAULT_PROP_TOUCH_ENABLE                     "0"

#define DEFAULT_PROP_SHIFT_S_DATE                     "0"
#define DEFAULT_PROP_SHIFT_S_TIME                     "0"
#define DEFAULT_PROP_SHIFT_E_DATE                     "0"
#define DEFAULT_PROP_SHIFT_E_TIME                     "0"
#define DEFAULT_PROP_SHIFT_LATEST                     "0"
#define DEFAULT_PROP_SHIFT_ENABLE                     "0"
#define DEFAULT_PROP_SHIFT_ACTIVE                     "0"

#define DEFAULT_PROP_MAX_AMNT_EN                      "0"
#define DEFAULT_PROP_MAX_AMNT                         "999999999999/0"

#define DEFAULT_PROP_DIRECT_SALE_EN                   "0"

#define DEFAULT_PROP_SERVICES_EN                      "0"

#define DEFAULT_PROP_FIXED_AMNT_ITEM                  "0"
#define DEFAULT_PROP_FIXED_AMNT_LIST                  "0"
#define DEFAULT_PROP_FIXED_AMNT_COEF                  "0"
#define DEFAULT_PROP_AMNT_LIST_CNT                    "0"

#define DEFAULT_PROP_SSL_EN                           "0"

#define DEFAULT_PROP_MAIN_SERVER_ID                   "0"
#define DEFAULT_PROP_TMS_ID                           "0"

#define DEFAULT_PROP_MERCHANT_PIN                     MERCHANT_DEFAULT_PIN

#define MERCHANT_PIN_LEN   4
#define ADMIN_PWD_LEN      6
#define SYS_PWD_LEN        8
#define SAFE_PWD_LEN       8

#define MAX_OPERATOR_NUM   4

static const DataDescriptor settingsDsc[] = {

    // 🔹 Terminal - basic
    DSC_BYTE(PROP_TOUCH_ENABLE, DEFAULT_PROP_TOUCH_ENABLE, __settings.terminal.touchEnable),
    DSC_BYTE(PROP_COMM_MODE, DEFAULT_PROP_COMM_MODE, __settings.terminal.netRoute),
    DSC_BYTE(PROP_KEY_VOLUME, DEFAULT_PROP_KEY_VOLUME, __settings.terminal.devVolume),
    DSC_BYTE(PROP_TIMEOUT_SLEEP, DEFAULT_PROP_TIMEOUT_SLEEP, __settings.terminal.sleepTimeout),
    DSC_BYTE(PROP_BRIGHTNESS, DEFAULT_PROP_BRIGHTNESS, __settings.terminal.brightness),

    // 🔹 Login / identity
    DSC_BYTE(PROP_LOGIN_OPERATOR, "101", __settings.terminal.loginOperator),
    DSC_STR_BUF(PROP_LOGIN_DATE, __settings.terminal.loginDate, ""),
    DSC_STR_BUF(PROP_MERCHANT_PIN, __settings.terminal.merchantPin, DEFAULT_PROP_MERCHANT_PIN),
    DSC_STR_BUF(PROP_MERCHANT_NO, __settings.terminal.merchantNo, ""),
    DSC_STR_BUF(PROP_TERMINAL_NO, __settings.terminal.terminalNo, ""),
    DSC_STR_BUF(PROP_MERCHANT_NAME, __settings.terminal.merchantName, ""),

    // 🔹 WiFi
    DSC_STR_BUF(PROP_WIFI_SSID, __settings.terminal.wfiSSID, ""),
    DSC_STR_BUF(PROP_WIFI_MAC, __settings.terminal.wifiMac, ""),
    DSC_INT(PROP_WIFI_ENC, "", __settings.terminal.wifiEnc),
    DSC_STR_BUF(PROP_WIFI_PWD, __settings.terminal.wifiPwd, ""),

    // 🔹 App
    DSC_INT(PROP_APP_ID, "", __settings.terminal.appID),

    // 🔹 Main server
    DSC_STR_BUF(PROP_MAIN_SERVER_IP, __settings.server.mainServerIp, DEFAULT_SERVER_IP),
    DSC_INT(PROP_MAIN_SERVER_PORT, DEFAULT_SERVER_PORT, __settings.server.mainServerPort),

    // 🔹 Backup server
    DSC_STR_BUF(PROP_BACKUP_SERVER_IP,  __settings.server.backupServerIp, DEFAULT_BACKUP_SERVER_IP),
    DSC_INT(PROP_BACKUP_SERVER_PORT, DEFAULT_BACKUP_SERVER_PORT, __settings.server.backupServerPort),

    // 🔹 Server IDs
    DSC_INT(PROP_MAIN_SERVER_ID, DEFAULT_PROP_MAIN_SERVER_ID, __settings.server.mainServerId),
    DSC_INT(PROP_TMS_ID, DEFAULT_PROP_TMS_ID, __settings.server.tmsId),

    DSC_BYTE(PROP_USE_BACKUP_FIRST, "0", __settings.server.useBackupAddressFirst),

    // 🔹 TMS
    DSC_STR_BUF(PROP_TMS_IP, __settings.server.tmsIp, DEFAULT_TMS_IP),
    DSC_INT(PROP_TMS_PORT, DEFAULT_TMS_PORT, __settings.server.tmsPort),
    DSC_STR_BUF(PROP_TMS_BACKUP_IP, __settings.server.tmsBackupIp, DEFAULT_TMS_IP),
    DSC_INT(PROP_TMS_BACKUP_PORT, DEFAULT_TMS_PORT, __settings.server.tmsBackupPort),

    DSC_BYTE(PROP_SSL_EN, DEFAULT_PROP_SSL_EN, __settings.server.sslEn),

    // 🔹 Terminal flags
    DSC_BYTE(PROP_LANGUAGE, DEFAULT_PROP_LANGUAGE, __settings.terminal.language),
    DSC_BYTE(PROP_PRINT_GREY_SCALE, DEFAULT_PROP_PRINT_GREY_SCALE, __settings.terminal.printGreyScale),
    DSC_BYTE(PROP_UPDATE_FLAG, DEFAULT_PROP_UPDATE_FLAG, __settings.terminal.updateFlag),
    DSC_BYTE(PROP_CONNECT_MODE,  DEFAULT_PROP_CONNECT_MODE, __settings.terminal.connectMode),

    // 🔹 Shift
    DSC_BIN(PROP_SHIFT_DATA, sizeof(__settings.shift.data), sizeof(__settings.shift.data), NULL, __settings.shift.data),

    DSC_INT(PROP_SHIFT_LATEST, DEFAULT_PROP_SHIFT_LATEST, __settings.shift.latest),
    DSC_INT(PROP_SHIFT_ENABLE, DEFAULT_PROP_SHIFT_ENABLE, __settings.shift.isEnable),
    DSC_INT(PROP_SHIFT_ACTIVE, DEFAULT_PROP_SHIFT_ACTIVE, __settings.shift.isActive),

    // 🔹 Amount settings
    DSC_INT(PROP_MAX_AMNT_EN, DEFAULT_PROP_MAX_AMNT_EN, __settings.terminal.maxAmntEnable),
    DSC_STR_BUF(PROP_MAX_AMNT, __settings.terminal.maxAmnt, DEFAULT_PROP_MAX_AMNT),

    DSC_BYTE(PROP_DIRECT_SALE_EN, DEFAULT_PROP_DIRECT_SALE_EN, __settings.terminal.directSaleEn),

    DSC_BIN(PROP_SERVICES_EN, sizeof(__settings.terminal.serviceEn), sizeof(__settings.terminal.serviceEn), NULL, __settings.terminal.serviceEn),

    DSC_INT(PROP_FIXED_AMNT_ITEM, DEFAULT_PROP_FIXED_AMNT_ITEM, __settings.terminal.fixedAmountItem),
    DSC_STR_BUF(PROP_FIXED_AMNT_LIST, __settings.terminal.amountList, DEFAULT_PROP_FIXED_AMNT_LIST),
    DSC_INT(PROP_FIXED_AMNT_COEF, DEFAULT_PROP_FIXED_AMNT_COEF, __settings.terminal.fixedAmountCoef),
    DSC_INT(PROP_AMNT_LIST_CNT, DEFAULT_PROP_AMNT_LIST_CNT, __settings.terminal.amountListCnt),
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