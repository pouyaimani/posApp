#ifdef DEVICE_TRENDITT3RTOS

#include "storage_t3Rtos.h"
#include "dev/dev.h"
#include "logger.h"
#include "sdkUtils.h"
#include "sdkemvapp.h"
#include "sdkFile.h"
#include "CONFIG.h"

#define USER_DATA_ROOT_DIR          "/mtd0/"

#define APP_DIR                     USER_DATA_ROOT_DIR
#define DEVICE_PROP_FILE            APP_DIR"lv_device_prop"

typedef enum {
    T_ASC = 0x00000001,                         // Store ASCII type data
    T_INT = 0x00000002,                         // Store int type data
    T_CHAR = 0x00000004,                        // Store char type data
} DATA_TYPE;

typedef struct _SettingItem {
    char *mTlvName;                            // Name stored in TLV
    DATA_TYPE mDataType;                       // Type of data stored
    uint32_t mMinLen;                               // Minimum space occupied by the data stored or minimum number of inputs
    uint32_t mMaxLen;                               // Maximum space occupied by the data stored or maximum number of inputs
    char *mDefaultValue;                       // Default value
    void *mAddress;                            // Address to read or store the corresponding data
} SettingItem;

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
#define PROP_OPERATOR_PWD                       "operator_pwd"
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

#define PROP_SHIFT_S_DATE                         "shift_start_date"
#define PROP_SHIFT_S_TIME                         "shift_start_time"
#define PROP_SHIFT_E_DATE                         "shift_end_date"
#define PROP_SHIFT_E_TIME                         "shift_end_time"
#define PROP_SHIFT_E_TIME                         "shift_end_time"
#define PROP_SHIFT_LATEST                         "shift_latest"
#define PROP_SHIFT_ENABLE                         "shift_enable"
#define PROP_SHIFT_ACTIVE                         "shift_active"

#define PROP_MAX_AMNT_EN                          "max_amnt_en"
#define PROP_MAX_AMNT                             "max_amnt"

#define PROP_DIRECT_SALE_EN                       "direct_sale_en"

#define PROP_SERVICES_EN                          "services_en"

#define VALUE_PROP_LANGUAGE                           libPropertiesGet(PROP_LANGUAGE)
#define VALUE_PROP_PRINT_GREY_SCALE                   libPropertiesGet(PROP_PRINT_GREY_SCALE)
#define VALUE_PROP_UPDATE_FLAG                        libPropertiesGet(PROP_UPDATE_FLAG)
#define VALUE_PROP_CONNECT_MODE                       libPropertiesGet(PROP_CONNECT_MODE)
#define VALUE_PROP_IS_OPEN_KEY_VOLUME                 libPropertiesGet(PROP_IS_OPEN_KEY_VOLUME)
#define VALUE_PROP_KEY_VOLUME                         libPropertiesGet(PROP_KEY_VOLUME)
#define VALUE_PROP_HAS_MARGIN                         libPropertiesGet(PROP_HAS_MARGIN)
#define VALUE_PROP_HAS_DOWNLOAD_TMK                   libPropertiesGet(PROP_HAS_DOWNLOAD_TMK)
#define VALUE_PROP_HAS_VERSION_FORCE_UPDATE           libPropertiesGet(PROP_HAS_VERSION_FORCE_UPDATE)
#define VALUE_PROP_TOUCH_ENABLE                       libPropertiesGet(PROP_TOUCH_ENABLE)
#define VALUE_PROP_COMM_MODE                          libPropertiesGet(PROP_COMM_MODE)
#define VALUE_PROP_TIMEOUT_SLEEP                      libPropertiesGet(PROP_TIMEOUT_SLEEP)
#define VALUE_PROP_BRIGHTNESS                         libPropertiesGet(PROP_BRIGHTNESS)
#define VALUE_PROP_LOGIN_OPERATOR                     libPropertiesGet(PROP_LOGIN_OPERATOR)
#define VALUE_PROP_HAS_LOGIN_ONLINE                   libPropertiesGet(PROP_HAS_LOGIN_ONLINE)
#define VALUE_PROP_LOGIN_DATE                         libPropertiesGetString(PROP_LOGIN_DATE)
#define VALUE_PROP_OPERATOR_PWD                       libPropertiesGetString(PROP_OPERATOR_PWD)
#define VALUE_PROP_99_PWD                             libPropertiesGetString(PROP_99_PWD)
#define VALUE_PROP_00_PWD                             libPropertiesGetString(PROP_00_PWD)
#define VALUE_PROP_SAFE_PWD                           libPropertiesGetString(PROP_SAFE_PWD)
#define VALUE_PROP_MERCHANT_NO                        libPropertiesGetString(PROP_MERCHANT_NO)
#define VALUE_PROP_TERMINAL_NO                        libPropertiesGetString(PROP_TERMINAL_NO)
#define VALUE_PROP_MERCHANT_NAME                      libPropertiesGetString(PROP_MERCHANT_NAME)
#define VALUE_PROP_BATCH_NO                           libPropertiesGetString(PROP_BATCH_NO)
#define VALUE_PROP_VOUCHER_NO                         libPropertiesGetString(PROP_VOUCHER_NO)
#define VALUE_PROP_CREATE_LINK_ATTR                   libPropertiesGetString(PROP_CREATE_LINK_ATTR)
#define VALUE_PROP_WIFI_SSID                          libPropertiesGetString(PROP_WIFI_SSID)
#define VALUE_PROP_WIFI_MAC                           libPropertiesGetString(PROP_WIFI_MAC)
#define VALUE_PROP_WIFI_ENC                           libPropertiesGet(PROP_WIFI_ENC)
#define VALUE_PROP_WIFI_PWD                           libPropertiesGetString(PROP_WIFI_PWD)
#define VALUE_PROP_TMS_CHECK_DATE                     libPropertiesGetString(PROP_TMS_CHECK_DATE)
#define VALUE_PROP_TIMEOUT_SHOW_ERR                   libPropertiesGet(PROP_TIMEOUT_SHOW_ERR)
#define VALUE_PROP_TIMEOUT_SHOW_TIP                   (libPropertiesGet(PROP_TIMEOUT_SHOW_TIP) * 1000)
#define VALUE_PROP_CREATE_PPP_TIMEOUT                 (libPropertiesGet(PROP_CREATE_PPP_TIMEOUT)  * 1000)
#define VALUE_PROP_TRANSMIT_ERR_TIPS_TIMEOUT          (libPropertiesGet(PROP_TRANSMIT_ERR_TIPS_TIMEOUT) * 1000)
#define VALUE_PROP_RECV_DATA_TIMEOUT                  (libPropertiesGet(PROP_RECV_DATA_TIMEOUT) * 1000)
#define VALUE_PROP_APP_ID                             libPropertiesGet(PROP_APP_ID)

#define VALUE_PROP_MAIN_SERVER_IP                     libPropertiesGetString(PROP_MAIN_SERVER_IP)
#define VALUE_PROP_MAIN_SERVER_PORT                   libPropertiesGetString(PROP_MAIN_SERVER_PORT)
#define VALUE_PROP_BACKUP_SERVER_IP                   libPropertiesGetString(PROP_BACKUP_SERVER_IP)
#define VALUE_PROP_BACKUP_SERVER_PORT                 libPropertiesGetString(PROP_BACKUP_SERVER_PORT)
#define VALUE_PROP_USE_BACKUP_FIRST                   libPropertiesGet(PROP_USE_BACKUP_FIRST)
#define VALUE_PROP_TMS_IP                             libPropertiesGetString(PROP_TMS_IP)
#define VALUE_PROP_TMS_PORT                           libPropertiesGetString(PROP_TMS_PORT)
#define VALUE_PROP_TMS_BACKUP_IP                      libPropertiesGetString(PROP_TMS_BACKUP_IP)
#define VALUE_PROP_TMS_BACKUP_PORT                    libPropertiesGetString(PROP_TMS_BACKUP_PORT)

#define VALUE_PROP_IS_RF_FIRST                        libPropertiesGet(PROP_IS_RF_FIRST)
#define VALUE_PROP_IS_SUPPORT_QPS                     libPropertiesGet(PROP_IS_SUPPORT_QPS)
#define VALUE_PROP_IS_SUPPORT_NO_SIGN                 libPropertiesGet(PROP_IS_SUPPORT_NO_SIGN)
#define VALUE_PROP_QPS_NO_PIN_AMOUNT                  libPropertiesGetString(PROP_QPS_NO_PIN_AMOUNT)
#define VALUE_PROP_QPS_NO_SIGN_AMOUNT                 libPropertiesGetString(PROP_QPS_NO_SIGN_AMOUNT)
#define VALUE_PROP_IS_QPS_BINA                        libPropertiesGet(PROP_IS_QPS_BINA)
#define VALUE_PROP_IS_QPS_BINB                        libPropertiesGet(PROP_IS_QPS_BINB)
#define VALUE_PROP_IS_QPS_CDCVM                       libPropertiesGet(PROP_IS_QPS_CDCVM)
#define VALUE_PROP_IS_SUPPORT_MAG_12_PWD              libPropertiesGet(PROP_IS_SUPPORT_MAG_12_PWD)
#define VALUE_PROP_IS_SUPPORT_EMPTY_PWD               libPropertiesGet(PROP_IS_SUPPORT_EMPTY_PWD)
#define VALUE_PROP_IS_WK_3DES                         libPropertiesGet(PROP_IS_WK_3DES)
#define VALUE_PROP_IS_SUPPORT_TDK                     libPropertiesGet(PROP_IS_SUPPORT_TDK)
#define VALUE_PROP_IS_AUTO_LOGOUT                     libPropertiesGet(PROP_IS_AUTO_LOGOUT)
#define VALUE_PROP_ICC_AID_COUNT                      libPropertiesGet(PROP_ICC_AID_COUNT)
#define VALUE_PROP_ICC_CAPK_COUNT                     libPropertiesGet(PROP_ICC_CAPK_COUNT)
#define VALUE_PROP_TMK_INDEX                          libPropertiesGet(PROP_TMK_INDEX)
#define VALUE_PROP_WK_INDEX                           libPropertiesGet(PROP_WK_INDEX)
#define VALUE_PROP_IS_PRINT_DETAIL                    libPropertiesGet(PROP_IS_PRINT_DETAIL)
#define VALUE_PROP_PRINT_TIMES                        libPropertiesGet(PROP_PRINT_TIMES)
#define VALUE_PROP_PRINT_TIMES_QRCODE                 libPropertiesGet(PROP_PRINT_TIMES_QRCODE)
#define VALUE_PROP_REVERSAL_TIMES                     libPropertiesGet(PROP_REVERSAL_TIMES)
#define VALUE_PROP_TPDU                               libPropertiesGetString(PROP_TPDU)
#define VALUE_PROP_IS_SUPPORT_GM                      libPropertiesGet(PROP_IS_SUPPORT_GM)
#define VALUE_PROP_IS_VERIFY_MAC                      libPropertiesGet(PROP_IS_VERIFY_MAC)

#define VALUE_PROP_SUPPORT_AUTO_LOGIN                 libPropertiesGet(PROP_SUPPORT_AUTO_LOGIN)
#define VALUE_PROP_SUPPORT_REVERSE                    libPropertiesGet(PROP_SUPPORT_REVERSE)
#define VALUE_PROP_SUPPORT_ES_BEFORE_ONLINE           libPropertiesGet(PROP_SUPPORT_ES_BEFORE_ONLINE)

#define VALUE_PROP_SHIFT_S_DATE                       libPropertiesGet(PROP_SHIFT_S_DATE)
#define VALUE_PROP_SHIFT_S_TIME                       libPropertiesGet(PROP_SHIFT_S_TIME)
#define VALUE_PROP_SHIFT_E_DATE                       libPropertiesGet(PROP_SHIFT_E_DATE)
#define VALUE_PROP_SHIFT_E_TIME                       libPropertiesGet(PROP_SHIFT_E_TIME)
#define VALUE_PROP_SHIFT_LATEST                       libPropertiesGet(PROP_SHIFT_LATEST)
#define VALUE_PROP_SHIFT_ENABLE                       libPropertiesGet(PROP_SHIFT_ENABLE)
#define VALUE_PROP_SHIFT_ACTIVE                       libPropertiesGet(PROP_SHIFT_ACTIVE)

#define VALUE_PROP_MAX_AMNT_EN                        libPropertiesGet(PROP_MAX_AMNT_EN)
#define VALUE_PROP_MAX_AMNT                           libPropertiesGet(PROP_MAX_AMNT)

#define VALUE_PROP_DIRECT_SALE_EN                     libPropertiesGet(PROP_DIRECT_SALE_EN)

#define VALUE_PROP_SERVICES_EN                        libPropertiesGet(PROP_SERVICES_EN)

#define DEFAULT_PROP_SHIFT_S_DATE                     0
#define DEFAULT_PROP_SHIFT_S_TIME                     0
#define DEFAULT_PROP_SHIFT_E_DATE                     0
#define DEFAULT_PROP_SHIFT_E_TIME                     0
#define DEFAULT_PROP_SHIFT_LATEST                     0
#define DEFAULT_PROP_SHIFT_ENABLE                     0
#define DEFAULT_PROP_SHIFT_ACTIVE                     0

#define DEFAULT_PROP_MAX_AMNT_EN                      0
#define DEFAULT_PROP_MAX_AMNT                         "999999999999/0"

#define DEFAULT_PROP_DIRECT_SALE_EN                   0

#define DEFAULT_PROP_SERVICES_EN                      0

#define OPERATOR_PWD_LEN   4
#define ADMIN_PWD_LEN      6
#define SYS_PWD_LEN        8
#define SAFE_PWD_LEN       8

#define MAX_OPERATOR_NUM   4


extern DevSettings _settings;

static const SettingItem settingsTable[] = {
    {
        PROP_IS_OPEN_KEY_VOLUME,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_OPEN_KEY_VOLUME,
        &_settings.terminal.mIsKeyVolumeOpen,
    },
    {
        PROP_HAS_MARGIN,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_HAS_MARGIN,
        &_settings.terminal.mHasMargined,
    },
    {
        PROP_HAS_DOWNLOAD_TMK,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_HAS_DOWNLOAD_TMK,
        &_settings.terminal.mHasDownloadTmk,
    },
    {
        PROP_HAS_VERSION_FORCE_UPDATE,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_HAS_VERSION_FORCE_UPDATE,
        &_settings.terminal.mHasVersionForceUpdate,
    },
    {
        PROP_TOUCH_ENABLE,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_TOUCH_ENABLE,
        &_settings.terminal.mTouchEnable,
    },
    {
        PROP_COMM_MODE,
        T_CHAR,
        0,
        4,
        DEFAULT_PROP_COMM_MODE,
        (void *) &_settings.terminal.netRoute,
    },
    {
        PROP_KEY_VOLUME,
        T_CHAR,
        0,
        6,
        DEFAULT_PROP_KEY_VOLUME,
        &_settings.terminal.devVolume,
    },
    {
        PROP_TIMEOUT_SLEEP,
        T_CHAR,
        0,
        255,
        DEFAULT_PROP_TIMEOUT_SLEEP,
        &_settings.terminal.mSleepTimeout,
    },
    {
        PROP_BRIGHTNESS,
        T_CHAR,
        1,
        5,
        DEFAULT_PROP_BRIGHTNESS,
        &_settings.terminal.brightness,
    },
    {
        PROP_LOGIN_OPERATOR,
        T_CHAR,
        0,
        101,
        "101",
        &_settings.terminal.mLoginOperator,
    },
    {
        PROP_HAS_LOGIN_ONLINE,
        T_CHAR,
        0,
        1,
        "0",
        &_settings.terminal.mHasLoginOnline,
    },
    {
        PROP_LOGIN_DATE,
        T_ASC,
        0,
        sizeof(_settings.terminal.mLoginDate) - 1,
        "",
        _settings.terminal.mLoginDate,
    },
    {
        PROP_OPERATOR_PWD,
        T_ASC,
        0,
        sizeof(_settings.terminal.mOperatePwd) - 1,
        DEFAULT_PROP_OPERATOR_PWD,
        _settings.terminal.mOperatePwd,
    },
    {
        PROP_99_PWD,
        T_ASC,
        8,
        sizeof(_settings.terminal.m99Pwd) - 1,
        DEFAULT_PROP_99_PWD,
        _settings.terminal.m99Pwd,
    },
    {
        PROP_00_PWD,
        T_ASC,
        6,
        sizeof(_settings.terminal.m00Pwd) - 1,
        DEFAULT_PROP_00_PWD,
        _settings.terminal.m00Pwd,
    },
    {
        PROP_MERCHANT_NO,
        (u32) T_ASC,
        15,
        sizeof(_settings.terminal.mMerchantNo) - 1,
        "",
        _settings.terminal.mMerchantNo,
    },
    {
        PROP_TERMINAL_NO,
        T_ASC,
        8,
        sizeof(_settings.terminal.mTerminalNo) - 1,
        "",
        _settings.terminal.mTerminalNo,
    },
    {
        PROP_MERCHANT_NAME,
        T_ASC,
        0,
        sizeof(_settings.terminal.mMerchantName) - 1,
        "",
        _settings.terminal.mMerchantName,
    },
    {
        PROP_BATCH_NO,
        T_ASC,
        0,
        sizeof(_settings.terminal.mBatchNO) - 1,
        "000001",
        _settings.terminal.mBatchNO,
    },
    {
        PROP_VOUCHER_NO,
        T_ASC,
        0,
        sizeof(_settings.terminal.mVoucherNO) - 1,
        "000001",
        _settings.terminal.mVoucherNO,
    },
    {
        PROP_SAFE_PWD,
        T_ASC,
        6,
        sizeof(_settings.terminal.mSafePwd) - 1,
        DEFAULT_PROP_SAFE_PWD,
        _settings.terminal.mSafePwd,
    },
    {
        PROP_CREATE_LINK_ATTR,
        T_ASC,
        0,
        sizeof(_settings.terminal.mTransmitCreateLinkAttr) - 1,
        DEFAULT_PROP_CREATE_LINK_ATTR,
        _settings.terminal.mTransmitCreateLinkAttr,
    },
    {
        PROP_WIFI_SSID,
        T_ASC,
        0,
        sizeof(_settings.terminal.mWifiSSID) - 1,
        "",
        _settings.terminal.mWifiSSID,
    },
    {
        PROP_WIFI_MAC,
        T_ASC,
        0,
        sizeof(_settings.terminal.mWifiMac) - 1,
        "",
        _settings.terminal.mWifiMac,
    },
    {
        PROP_WIFI_ENC,
        T_INT,
        0,
        0,
        "",
        (void *)&_settings.terminal.mWifiEnc,
    },
    {
        PROP_WIFI_PWD,
        T_ASC,
        0,
        sizeof(_settings.terminal.mWifiPwd) - 1,
        "",
        _settings.terminal.mWifiPwd,
    },
    {
        PROP_TMS_CHECK_DATE,
        T_ASC,
        0,
        sizeof(_settings.terminal.mTmsCheckDate) - 1,
        "",
        _settings.terminal.mTmsCheckDate,
    },
    {
        PROP_TIMEOUT_SHOW_ERR,
        T_INT,
        0,
        0,
        DEFAULT_PROP_TIMEOUT_SHOW_ERR,
        (void *) &_settings.terminal.mTimeoutShowErr,
    },
    {
        PROP_TIMEOUT_SHOW_TIP,
        T_INT,
        0,
        0,
        DEFAULT_PROP_TIMEOUT_SHOW_TIP,
        (void *) &_settings.terminal.mTimeoutShowTip,
    },
    {
        PROP_CREATE_PPP_TIMEOUT,
        T_INT,
        0,
        0,
        DEFAULT_PROP_CREATE_PPP_TIMEOUT,
        (void *) &_settings.terminal.mTransmitCreatePPPTimeout,
    },
    {
        PROP_TRANSMIT_ERR_TIPS_TIMEOUT,
        T_INT,
        0,
        0,
        DEFAULT_PROP_TRANSMIT_ERR_TIPS_TIMEOUT,
        (void *) &_settings.terminal.mTransmitErrTipsTimeout,
    },
    {
        PROP_RECV_DATA_TIMEOUT,
        T_INT,
        0,
        0,
        DEFAULT_PROP_RECV_DATA_TIMEOUT,
        (void *) &_settings.terminal.mRecvDataTimeout,
    },
    {
        PROP_APP_ID,
        T_INT,
        0,
        0,
        "",
        (void *) &_settings.terminal.mAppID,
    },
    {
        PROP_MAIN_SERVER_IP,
        T_ASC,
        0,
        sizeof(_settings.server.mMainServerIp) - 1,
        SERVER_IP,
        _settings.server.mMainServerIp,
    },
    {
        PROP_MAIN_SERVER_PORT,
        T_ASC,
        1,
        sizeof(_settings.server.mMainServerPort) - 1,
        SERVER_PORT,
        _settings.server.mMainServerPort,
    },
    {
        PROP_BACKUP_SERVER_IP,
        T_ASC,
        0,
        sizeof(_settings.server.mBackupServerBackupIp) - 1,
        BACKUP_SERVER_IP,
        _settings.server.mBackupServerBackupIp,
    },
    {
        PROP_BACKUP_SERVER_PORT,
        T_ASC,
        1,
        sizeof(_settings.server.mBackupServerBackupPort) - 1,
        BACKUP_SERVER_PORT,
        _settings.server.mBackupServerBackupPort,
    },
    {
        PROP_USE_BACKUP_FIRST,
        T_CHAR,
        0,
        1,
        "0",
        &_settings.server.mUseBackupAddressFirst,
    },
    {
        PROP_TMS_IP,
        T_ASC,
        0,
        sizeof(_settings.server.mTmsIp) - 1,
        TMS_IP,
        _settings.server.mTmsIp,
    },
    {
        PROP_TMS_PORT,
        T_ASC,
        1,
        sizeof(_settings.server.mTmsPort) - 1,
        TMS_PORT,
        _settings.server.mTmsPort,
    },
    {
        PROP_TMS_BACKUP_IP,
        T_ASC,
        0,
        sizeof(_settings.server.mTmsBackupIp) - 1,
        TMS_IP,
        _settings.server.mTmsBackupIp,
    },
    {
        PROP_TMS_BACKUP_PORT,
        T_ASC,
        1,
        sizeof(_settings.server.mTmsBackupPort) - 1,
        TMS_PORT,
        _settings.server.mTmsBackupPort,
    },
    {
        PROP_IS_SUPPORT_QPS,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_SUPPORT_QPS,
        &_settings.txn.mIsSupportQPS,
    },
    {
        PROP_IS_SUPPORT_NO_SIGN,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_SUPPORT_NO_SIGN,
        &_settings.txn.mIsSupportNoSign,
    },
    {
        PROP_IS_SUPPORT_MAG_12_PWD,
        T_CHAR,
        0,
        0,
        DEFAULT_PROP_IS_SUPPORT_MAG_12_PWD,
        (void *) &_settings.txn.mIsSupportMag12Pwd,
    },
    {
        PROP_IS_SUPPORT_EMPTY_PWD,
        T_CHAR,
        0,
        0,
        DEFAULT_PROP_IS_SUPPORT_EMPTY_PWD,
        (void *) &_settings.txn.mIsSupportEmptyPwd,
    },
    {
        PROP_IS_QPS_BINA,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_QPS_BINA,
        &_settings.txn.mIsQPSBINA,
    },
    {
        PROP_IS_QPS_BINB,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_QPS_BINB,
        &_settings.txn.mIsQPSBINB,
    },
    {
        PROP_IS_QPS_CDCVM,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_QPS_CDCVM,
        &_settings.txn.mIsQPSCDCVM,
    },
    {
        PROP_IS_RF_FIRST,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_RF_FIRST,
        (void *) &_settings.txn.mIsRFFirst,
    },
    {
        PROP_IS_WK_3DES,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_WK_3DES,
        &_settings.txn.mIsMk3Des,
    },
    {
        PROP_IS_AUTO_LOGOUT,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_AUTO_LOGOUT,
        &_settings.txn.mIsAutoLogout,
    },
    {
        PROP_ICC_AID_COUNT,
        T_CHAR,
        0,
        255,
        "0",
        &_settings.txn.mIccAidCount,
    },
    {
        PROP_ICC_CAPK_COUNT,
        T_CHAR,
        0,
        255,
        "0",
        &_settings.txn.mIccCAPKCount,
    },
    {
        PROP_TMK_INDEX,
        T_CHAR,
        0,
        9,
        "0",
        &_settings.txn.mTMkIndex,
    },
    {
        PROP_WK_INDEX,
        T_CHAR,
        0,
        0,
        "0",
        (void *) &_settings.txn.mWKIndex,
    },
    {
        PROP_IS_PRINT_DETAIL,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_PRINT_DETAIL,
        &_settings.txn.mIsPrintDetail,
    },
    {
        PROP_PRINT_TIMES,
        T_CHAR,
#ifdef MIN_PRINT_TIMES
        MIN_PRINT_TIMES,
#else
        0,
#endif
        3,
        DEFAULT_PROP_PRINT_TIMES,
        &_settings.txn.mPrintTimes,
    },
    {
        PROP_REVERSAL_TIMES,
        T_CHAR,
        1,
        3,
        DEFAULT_PROP_REVERSAL_TIMES,
        &_settings.txn.mReversalTimes,
    },
    {
        PROP_TPDU,
        T_ASC,
        10,
        sizeof(_settings.txn.mTPDU) - 1,
        DEFAULT_PROP_TPDU,
        _settings.txn.mTPDU,
    },
    {
        PROP_QPS_NO_PIN_AMOUNT,
        T_ASC,
        12,
        12,
        DEFAULT_PROP_QPS_NO_PIN_AMOUNT,
        &_settings.txn.mQPSNoPinAmount,
    },
    {
        PROP_QPS_NO_SIGN_AMOUNT,
        T_ASC,
        12,
        12,
        DEFAULT_PROP_QPS_NO_SIGN_AMOUNT,
        &_settings.txn.mQPSNoSignAmount,
    },
    {
        PROP_IS_SUPPORT_GM,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_SUPPORT_GM,
        &_settings.txn.mIsSupportGM,
    },
    {
        PROP_IS_VERIFY_MAC,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_VERIFY_MAC,
        &_settings.txn.mIsVerifyMacField,
    },
    {
        PROP_SUPPORT_AUTO_LOGIN,
        T_CHAR,
        0,
        1,
        SUPPORT_AUTO_LOGIN,
        &_settings.terminal.mSupportAutoLogin,
    },
    {
        PROP_SUPPORT_REVERSE,
        T_CHAR,
        0,
        1,
        SUPPORT_REVERSE,
        &_settings.terminal.mSupportReverse,
    },
    {
        PROP_SUPPORT_ES_BEFORE_ONLINE,
        T_CHAR,
        0,
        1,
        SUPPORT_ES_BEFORE_TRANS,
        &_settings.terminal.mSupportEsBeforeOnline,
    },
    {
        PROP_IS_SUPPORT_TDK,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_IS_WK_3DES,
        &_settings.txn.mIsSupportTdk,
    },
    {
        PROP_PRINT_TIMES_QRCODE,
        T_CHAR,
#ifdef MIN_PRINT_TIMES
        MIN_PRINT_TIMES,
#else
        0,
#endif
        3,
#ifdef DEFAULT_PROP_PRINT_TIMES_QRCODE
        DEFAULT_PROP_PRINT_TIMES_QRCODE,
#else
        DEFAULT_PROP_PRINT_TIMES,
#endif
        &_settings.txn.mPrintTimesQrcode,
    },
    {
        PROP_LANGUAGE,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_LANGUAGE,
        &_settings.terminal.mLanguage,
    },
    {
        PROP_PRINT_GREY_SCALE,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_PRINT_GREY_SCALE,
        &_settings.terminal.mPrintGreyScale,
    },
    {
        PROP_UPDATE_FLAG,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_UPDATE_FLAG,
        &_settings.terminal.mUpdateFlag,
    },
    {
        PROP_CONNECT_MODE,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_CONNECT_MODE,
        &_settings.terminal.mConnectMode,
    },
    {
        PROP_SHIFT_S_DATE,
        T_INT,
        0,
        sizeof(uint32_t) * MAX_SHIFT_CNT,
        DEFAULT_PROP_SHIFT_S_DATE,
        &_settings.shift.data->startDate,
    },
    {
        PROP_SHIFT_S_TIME,
        T_INT,
        0,
        sizeof(uint32_t) * MAX_SHIFT_CNT,
        DEFAULT_PROP_SHIFT_S_TIME,
        &_settings.shift.data->startTime,
    },
    {
        PROP_SHIFT_E_DATE,
        T_INT,
        0,
        sizeof(uint32_t) * MAX_SHIFT_CNT,
        DEFAULT_PROP_SHIFT_E_DATE,
        &_settings.shift.data->endDate,
    },
    {
        PROP_SHIFT_E_TIME,
        T_INT,
        0,
        sizeof(uint32_t) * MAX_SHIFT_CNT,
        DEFAULT_PROP_SHIFT_E_TIME,
        &_settings.shift.data->endTime,
    },
    {
        PROP_SHIFT_LATEST,
        T_INT,
        0,
        sizeof(uint16_t),
        DEFAULT_PROP_SHIFT_LATEST,
        &_settings.shift.latest,
    },
    {
        PROP_SHIFT_ENABLE,
        T_INT,
        0,
        sizeof(uint8_t),
        DEFAULT_PROP_SHIFT_ENABLE,
        &_settings.shift.isEnable,
    },
    {
        PROP_SHIFT_ACTIVE,
        T_INT,
        0,
        sizeof(uint8_t),
        DEFAULT_PROP_SHIFT_ACTIVE,
        &_settings.shift.isActive,
    },
    {
        PROP_MAX_AMNT_EN,
        T_INT,
        0,
        sizeof(uint8_t),
        DEFAULT_PROP_MAX_AMNT_EN,
        &_settings.terminal.maxAmntEnable,
    },
    {
        PROP_MAX_AMNT,
        T_ASC,
        0,
        13,
        DEFAULT_PROP_MAX_AMNT,
        &_settings.terminal.maxAmnt,
    },
    {
        PROP_DIRECT_SALE_EN,
        T_CHAR,
        0,
        1,
        DEFAULT_PROP_DIRECT_SALE_EN,
        &_settings.terminal.directSaleEn,
    },
    {
        PROP_SERVICES_EN,
        T_INT,
        MAX_SERVICE_NUM,
        MAX_SERVICE_NUM,
        DEFAULT_PROP_SERVICES_EN,
        _settings.terminal.serviceEn,
    },
};

static uint16_t getDevicePropsTableCount(void)
{
    return sizeof(settingsTable) / sizeof(SettingItem);
}

static uint16_t formTlv(uint8_t *out, const u8 *tag, const u8 *value, uint16_t valueLen)
{
    u8 uc_len = 0;
    uint16_t us_len = 0;

    *out = 0xFF;                   
    us_len++;

    uc_len = (u8) strlen((const char *) tag);
    out[us_len++] = uc_len;        

    memcpy(out + us_len, tag, uc_len);    //Tag
    us_len += uc_len;

    if (valueLen > 0)
    {
        if (valueLen > 255)
        {
            out[us_len++] = 0x82;
            out[us_len++] = (u8) (valueLen / 256);
        }
        else if (valueLen > 127)
        {
            out[us_len++] = 0x81;
        }
        out[us_len++] = (u8) valueLen;
        memcpy(&out[us_len], value, valueLen);
        us_len += valueLen;
    }
    else
    {
        us_len = 0;
    }
    return us_len;
}

static s32 parseTLVData(const u8 *tlvData, uint16_t tlvDataLen, SettingItem *settings, uint32_t itemsCount)
{
    char tlvTag[32] = {0};
    bool isMatchTlvTag = false;
    uint32_t offset = 0, us_len = 0, us_temp = 0, i, j;

    while (offset < tlvDataLen)
    {
        if (offset >= tlvDataLen)
        {
            break;
        }

        if (tlvData[offset] == 0xFF)
        {
            offset++;
            continue;
        }
        isMatchTlvTag = false;

        us_len = tlvData[offset];     
        offset++;

        memset(tlvTag, 0, sizeof(tlvTag));
        memcpy(tlvTag, tlvData + offset, us_len);
        for (i = 0; i < itemsCount; i++)
        {
            if (strcmp(tlvTag, settings[i].mTlvName) != 0)
            {
                continue;
            }
            isMatchTlvTag = true;
            break;
        }

        offset += us_len;

        if (tlvData[offset] <= 127)        //0x7F
        {
            us_len = tlvData[offset];
            offset++;
        }
        else
        {
            us_len = 0;
            us_temp = (uint16_t) (tlvData[offset] & 0x7Fu);

            for (j = 1; j <= us_temp; j++)
            {
                us_len = (uint16_t) (us_len * 256 + tlvData[offset + j]);
            }

            offset += us_temp + 1;
        }

        if (isMatchTlvTag)
        {

            if ((settings[i].mDataType & (uint32_t) T_INT) == T_INT)
            {
                if (us_len != sizeof(uint32_t)) return SDK_ERR;
            }
            else if ((settings[i].mDataType & (uint32_t) T_CHAR) == T_CHAR)
            {
                if (us_len != sizeof(u8)) return SDK_ERR;
            }
            else
            {
                if (us_len > settings[i].mMaxLen || us_len < settings[i].mMinLen) return SDK_ERR;
                memset((u8 *) settings[i].mAddress, 0, settings[i].mMaxLen + 1);
            }

            memcpy((u8 *) settings[i].mAddress, tlvData + offset, us_len);

        }
        offset += us_len;
    }

    return SDK_OK;
}

static void applySettings(char *fileName, SettingItem *settings, uint32_t itemsCount)
{
    u8 *fileWriteBuf = NULL;
    uint32_t itemWriteLen = 0;
    uint16_t crc16 = 0;
    uint32_t i = 0;
    uint16_t writeBufLen = 0;
    s32 ret = 0;

    if (fileName == NULL || strlen(fileName) == 0)
    {
        LOG_ERROR("applySettings err, fileName format err ...\r\n");
        return;
    }

    writeBufLen += SETTINGS_FILE_HEADER_LEN;

    fileWriteBuf = (u8 *) sdkSysGetMem(SETTINGS_FILE_MAX_SIZE);
    if (fileWriteBuf == NULL)
    {
        return;
    }

    memset(fileWriteBuf, 0, SETTINGS_FILE_MAX_SIZE);
    for (i = 0; i < itemsCount; i++)
    {
        if ((settings[i].mDataType & (uint32_t) T_INT) == T_INT)
        {
            itemWriteLen = sizeof(uint32_t);
        }
        else if ((settings[i].mDataType & (uint32_t) T_CHAR) == T_CHAR)
        {
            itemWriteLen = sizeof(u8);
        }
        else
        {
            itemWriteLen = settings[i].mMaxLen;
        }
        writeBufLen += formTlv(fileWriteBuf + writeBufLen, (u8 *) settings[i].mTlvName, (u8 *) settings[i].mAddress, (uint16_t) itemWriteLen);

        if (writeBufLen >= SETTINGS_FILE_MAX_SIZE)
        {
            break;
        }
    }

    crc16 = (uint16_t) libCrc16(fileWriteBuf + SETTINGS_FILE_HEADER_LEN, (uint32_t) (writeBufLen - SETTINGS_FILE_HEADER_LEN));
    fileWriteBuf[0] = (u8) ((writeBufLen - SETTINGS_FILE_HEADER_LEN) / 256);
    fileWriteBuf[1] = (u8) (writeBufLen - SETTINGS_FILE_HEADER_LEN % 256);
    fileWriteBuf[2] = (u8) (crc16 >> 8u);
    fileWriteBuf[3] = (u8) (crc16 & 0xFFu);
    ret = sdkFileWrite(fileName, fileWriteBuf, writeBufLen);
    // LOG_INFO("sdkFileWrite applySettings  ret=%d [%s] len=%d   \n", ret, fileName, writeBufLen);
    platformSync();
    sdkSysFreeMem(fileWriteBuf);
}

static void initSettings(SettingItem *settings, uint32_t itemsCount)
{
    uint32_t i = 0;
    uint32_t intDefaultValue = 0;

    for (i = 0; i < itemsCount; i++)
    {
        if ((settings[i].mDataType & (uint32_t) T_INT) == T_INT)
        {
            intDefaultValue = (uint32_t) libAtoi(settings[i].mDefaultValue);
            memcpy(settings[i].mAddress, &intDefaultValue, sizeof(uint32_t));
//            LOG_INFO("%d.%20s:%d\r\n", i, settings[i].mTlvName, intDefaultValue);
        }
        else if ((settings[i].mDataType & (uint32_t) T_CHAR) == T_CHAR)
        {
            intDefaultValue = (uint32_t) libAtoi(settings[i].mDefaultValue);
            memcpy(settings[i].mAddress, &intDefaultValue, sizeof(u8));
//            LOG_INFO("%d.%20s:%d\r\n", i, settings[i].mTlvName, intDefaultValue);
        }
        else
        {
            memset(settings[i].mAddress, 0, settings[i].mMaxLen + 1);

            if (settings[i].mDefaultValue != NULL)
            {
                memcpy(settings[i].mAddress, settings[i].mDefaultValue, strlen((const char *) settings[i].mDefaultValue));
//                LOG_INFO("%d.%20s:%s\r\n", i, settings[i].mTlvName, settings[i].mDefaultValue);
            }
        }
    }
}

static bool isSettingsFileCorrect(u8 *settingsData, uint32_t settingsDataLen, uint32_t crc)
{
    uint16_t calcCrc = 0;

    calcCrc = (uint16_t) libCrc16(settingsData, settingsDataLen);

    return calcCrc == crc ? true : false;
}

static s32 readSettingsFileHeadInfo(char *fileName, uint32_t *fileSize, uint32_t *fileCrc)
{
    uint32_t readSize = SETTINGS_FILE_HEADER_LEN;
    u8 fileSizeBytes[SETTINGS_FILE_HEADER_LEN] = {0};
    s32 ret = 0;

    ret = sdkFileRead(fileName, fileSizeBytes, 0, &readSize);
    if (ret != SDK_FILE_OK || readSize != SETTINGS_FILE_HEADER_LEN)
    {
        return SDK_ERR;
    }
    *fileSize = (uint32_t) (fileSizeBytes[0] * 256 + fileSizeBytes[1]);

    if (*fileSize > SETTINGS_FILE_MAX_SIZE || *fileSize == 0)
    {
        return SDK_ERR;
    }

    *fileCrc = (uint32_t) (fileSizeBytes[2] * 256 + fileSizeBytes[3]);

    return SDK_OK;
}

static void reloadSettings(char *fileName, SettingItem *settings, uint32_t itemsCount)
{
    u8 *fileCaches = NULL;
    uint32_t fileSize = 0;
    uint32_t fileCrc = 0;
    uint32_t readSize = 0;
    s32 ret = SDK_ERR;
    // goto init_settings;

    if (fileName == NULL || strlen(fileName) == 0)
    {
        LOG_ERROR("reloadSettings err , fileName is err format or func is null...\r\n");
        return;
    }

    ret = readSettingsFileHeadInfo(fileName, &fileSize, &fileCrc);
    if (ret != SDK_OK)
    {
        LOG_ERROR("read file head info err ret = %d filesize:%d\r\n", ret, fileSize);
        goto init_settings;
    }

    fileCaches = (u8 *) sdkSysGetMem(fileSize);
    if (fileCaches == NULL)
    {
        goto init_settings;
    }
    memset(fileCaches, 0, fileSize);
    readSize = fileSize;
    ret = sdkFileRead(fileName, fileCaches, SETTINGS_FILE_HEADER_LEN, &readSize);
    if (ret != SDK_FILE_OK || readSize != fileSize)
    {
        LOG_ERROR("read file %s err , ret = %d readSize = %d fileSize = %d \r\n", fileName, ret, readSize, fileSize);
        goto init_settings;
    }
    if (!isSettingsFileCorrect(fileCaches, fileSize, fileCrc))
    {
        LOG_ERROR("file verify is err \r\n");
        goto init_settings;
    }

    parseTLVData(fileCaches, (uint16_t) fileSize, settings, itemsCount);
    LOG_INFO("parse settings tlv data success ... \r\n");
    sdkSysFreeMem(fileCaches);
    return;

    init_settings:

    if (fileCaches != NULL) sdkSysFreeMem(fileCaches);

    initSettings(settings, itemsCount);
    applySettings(fileName, settings, itemsCount);
}

static SettingItem *getSettingItem(char *name, SettingItem *settings, uint32_t itemCount)
{
    s32 i = 0;
    SettingItem *item = NULL;

    for (i = 0; i < itemCount; i++)
    {
        if (0 == strcmp(name, settings[i].mTlvName))
        {
            item = &settings[i];
            break;
        }
    }

    return item;
}

static void _init(Storage *self) {

}

static void _reloadSettings(Storage *self) {
    memset(&_settings, 0x00, sizeof(DevSettings));
    reloadSettings(DEVICE_PROP_FILE, (SettingItem *) &settingsTable, getDevicePropsTableCount());
}

static void _applySettings(Storage *self) {
    applySettings(DEVICE_PROP_FILE, (SettingItem *) &settingsTable, getDevicePropsTableCount());
}

OOP_CTOR(StorageT3Rtos) {
    self->base.vtable.init = _init;
    self->base.vtable.reloadSettings = _reloadSettings;
    self->base.vtable.applySettings = _applySettings;
}

#endif
