#ifdef DEVICE_TRENDITT3RTOS

#include "sys_t3Rtos.h"
#include "posplatform.h"
#include "sdkSys.h"
#include "appVersion.h"
#include "LibProperties.h"
#include "sdkUtils.h"
#include "sdkLog.h"
#include "utility/arith.h"
#include "utility/utility.h"
#include "logger.h"
#include "error.h"

static DateTime dateTime;
extern BatteryStat batterySt;
static TerminalInfo tinfo;

//app address in ram
#define MCU_BASE_ADDR  (0x1000000)
#define APP_START      (0xD8000)
#define APP_CORE_START (APP_START + 0x200)
//end

//define APP_VER for coremanage to read
#define APP_VER "V"APPID DEVICE_MACHINE_ID APPVERSION    //VHWCS33231218001
const char ver[20] __attribute__((at(APP_START + MCU_BASE_ADDR))) = APP_VER;
//end

//define app_entry for coremanage to call
void appMain(void);
typedef void (*core_app_start)(void);
const core_app_start app_entry __attribute__((at(APP_CORE_START + MCU_BASE_ADDR))) = appMain;
//end

#define DEVICE_MACHINE_ID  "33"
#define USER_DATA_ROOT_DIR "/mtd0/"
#define APP_DIR             USER_DATA_ROOT_DIR APPID "/"
#define IMG_PATH "img/"

static void initRam(void)
{
    //Compiler-defined symbols
    extern unsigned int Image$$RW_IRAM1$$Base;
    extern unsigned int Image$$RW_IRAM1$$Length;
    extern unsigned int Image$$RW_IRAM1$$Limit;

    extern unsigned int Image$$ER_IROM1$$Base;
    extern unsigned int Image$$ER_IROM1$$Length;
    extern unsigned int Image$$ER_IROM1$$Limit;

    unsigned int *src = &Image$$ER_IROM1$$Limit;
    unsigned int *dst = &Image$$RW_IRAM1$$Base;
    unsigned int *end = src + ((unsigned int)&Image$$RW_IRAM1$$Length/sizeof(unsigned int));

    while (src < end)
    {
        *dst++ = *src++;
    }
}

void platformSysInit(void)
{
    initRam();

    sdkEmvSetAppDir(APP_DIR);
}

void platformSync(void) {}

static void init(System* dev) {
    platformSysInit();
    sdkSysInit();
    sdkLogSetLevel(4);
    
    dev->module.wifi = sdkSysIsDeviceExist(SYS_DEVICE_WIFI);
    dev->module.gprs = sdkSysIsDeviceExist(SYS_DEVICE_WIRELESS);
    dev->module.printer = sdkSysIsDeviceExist(SYS_DEVICE_PRINTER);
    dev->module.audio = sdkSysIsDeviceExist(SYS_DEVICE_AUDIO);
    dev->module.scanner = sdkSysIsDeviceExist(SYS_DEVICE_CAMERA);
    dev->module.bt = sdkSysIsDeviceExist(SYS_DEVICE_BLUETOOTH);
    dev->module.dialup = false;
    sdkSysReadTerminalInfo(&tinfo);
}

static void getTick(System* dev) {
    return sdkSysGetTicks();
}

static unsigned int getMemory(System* dev, unsigned int size) {
    return sdkSysGetMem(size);
}

static void freeMemory(System* dev, void *mem) {
    sdkSysFreeMem(mem);
}

static unsigned int flushDisplay(System* dev, int32_t x0, int32_t x1, int32_t y0, int32_t y1, uint8_t *cmap) {
    strRect rect = {
        .m_x0 = x0,
        .m_x1 = x1,
        .m_y0 = y0,
        .m_y1 = y1
    };
    strPicture pic = {
        .m_width = x1 - x0 + 1,
        .m_height = y1 - y0 + 1,
        .m_pic = cmap
    };
    ddi_lcd_show_picture(&rect, &pic);
}

static void logOut(System* dev, const char *data, size_t len, void *udata) {
    sdkLogOut(data);
}

static inline uint8_t bcdToDec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static void parseRtcTime(const uint8_t *rtcTime,
                  int *year,
                  int *month,
                  int *day,
                  int *hour,
                  int *minute,
                  int *second)
{
    *year  = 2000 + bcdToDec(rtcTime[0]);
    *month  = bcdToDec(rtcTime[1]);
    *day  = bcdToDec(rtcTime[2]);
    *hour  = bcdToDec(rtcTime[3]);
    *minute = bcdToDec(rtcTime[4]);
    *second  = bcdToDec(rtcTime[5]);
}

static DateTime *getDateTime(System* dev) {
    uint8_t dt[6];
    memset(dt, 0, sizeof(dt));
    char tmp[12 + 1] = {0};
    sdkSysGetRtcTime(dt);
    bcdToAsc(tmp, dt, 6);
    memcpy(dateTime.date, tmp, 6);
    memcpy(dateTime.time, tmp + 6, 6);
    return &dateTime;
}

static uint32_t getDate(System *dev) {
    uint8_t dt[12 + 1];
    memset(dt, 0, sizeof(dt));
    sdkSysGetRtcTime(dt);
    int year, month, day, hour, minutes, second;
    parseRtcTime(dt, &year, &month, &day, &hour, &minutes, &second);

    uint32_t date = (year * 10000) + (month) + day;
    return date;
}

static uint32_t getTime(System *dev) {
    uint8_t dt[12 + 1];
    memset(dt, 0, sizeof(dt));
    sdkSysGetRtcTime(dt);
    int year, month, day, hour, minutes, second;
    parseRtcTime(dt, &year, &month, &day, &hour, &minutes, &second);

    uint32_t time = (hour * 10000) + (minutes) + second;
    return time;
}

static uint64_t getPackedDateTime(System *dev) {
    uint8_t dt[12 + 1];
    memset(dt, 0, sizeof(dt));
    sdkSysGetRtcTime(dt);
    int year, month, day, hour, minutes, second;
    parseRtcTime(dt, &year, &month, &day, &hour, &minutes, &second);

    uint32_t date = (year * 10000) + (month) + day;
    uint32_t time = (hour * 10000) + (minutes) + second;

    return packDateTime(date, time);
}


static BatteryStat* getBatteryStatus(System *dev) {
    BatteryStatus st;
    int ret = sdkSysGetBatteryStatus(&st);
    if (st.mBatteryLevel == SYS_BATTERY_LEVEL_NULL) {
        batterySt.level = DEV_BAT_LEV_LOW;
    } else if (st.mBatteryLevel < SYS_BATTERY_LEVEL_3) {
        batterySt.level = DEV_BAT_LEV_1;
    } else if (st.mBatteryLevel == SYS_BATTERY_LEVEL_3) {
        batterySt.level = DEV_BAT_LEV_2;
    } else {
        batterySt.level = DEV_BAT_LEV_3;
    }
    batterySt.isChanrging = st.mIsCharging;
    batterySt.percent = st.mBatteryPercent;
    return &batterySt;
}

static void sysSleep(System *dev, uint32_t mili) {
    sdkSysSleep(mili);
}

static void reboot(System *dev) {
    sdkSysDeviceReboot();
}

static void powerOff(System *dev) {
    sdkSysDevicePowerOff();
}

static int8_t getSN(System *dev, char *out, size_t len) {
    memset(out, 0, sizeof(len));
    int8_t err = sdkSysReadDeviceSN(SYS_SN_TYPE_MANUFACTURER,
                     out, len);
    if (err != SDK_SYS_OK) {
        LOG_ERROR("SYS: failure in reading device serail number. error = %d", err);
        return ERR_NOK;
    }
    return ERR_OK;
}

static const char *getCode(System *dev) {
    return tinfo.mTerminalCode;
}

static const char *getName(System *dev) {
    return tinfo.mTerminalName;
}

static void setVolume(System *dev, int volume) {
    sdkSysSetDeviceVolume(SYS_VOLUME_TYPE_AUDIO, volume);
}

static void setBrightness(System *dev, int bright) {
    int br = bright > dev->maxBright ? dev->maxBright : bright;
    br = bright < 1 ? 1 : bright;
    ddi_lcd_ioctl(DDI_LCD_CTL_BRIGHT, br, 0);
}

static int getVolume(System *dev) {
    return sdkSysGetDeviceVolume(SYS_VOLUME_TYPE_AUDIO);
}

static int getBrightness(System *dev) {

}

static void beepOnce(System *dev) {
    sdkSysBeepOnce();
}

static void setDateTime(System *dev, DateTime *dt) {
    sdkSysBeepOnce();
}

void T3Rtos_ctor(T3Rtos* self, const char* name) {
    self->base.name = name;
    self->base.vtable.init = init;
    self->base.vtable.getTick = getTick;
    self->base.vtable.getMemory = getMemory;
    self->base.vtable.flushDisplay = flushDisplay;
    self->base.vtable.freeMemory = freeMemory;
    self->base.vtable.logOut = logOut;
    self->base.vtable.getDateTime = getDateTime;
    self->base.vtable.getBatteryStatus = getBatteryStatus;
    self->base.vtable.sleep = sysSleep;
    self->base.vtable.reboot = reboot;
    self->base.vtable.powerOff = powerOff;
    self->base.vtable.getSN = getSN;
    self->base.vtable.setVolume = setVolume;
    self->base.vtable.setBrightness = setBrightness;
    self->base.vtable.beepOnce = beepOnce;
    self->base.vtable.getVolume = getVolume;
    self->base.vtable.getBrightness = getBrightness;
    self->base.vtable.setDateTime = setDateTime;
    self->base.vtable.getCode = getCode;
    self->base.vtable.getName = getName;
    self->base.vtable.getDate = getDate;
    self->base.vtable.getTime = getTime;
    self->base.vtable.getPackedDateTime = getPackedDateTime;

    self->base.maxBright = 5;
    self->base.maxSound = 5;
}

#endif