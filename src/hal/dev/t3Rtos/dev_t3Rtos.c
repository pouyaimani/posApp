#ifdef DEVICE_TRENDITT3RTOS

#include "dev_t3Rtos.h"
#include "posplatform.h"
#include "sdkSys.h"
#include "appVersion.h"
#include "LibProperties.h"
#include "sdkUtils.h"
#include "sdkLog.h"

static DateTime dateTime;
extern BatteryStat batterySt;

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

    #if 0
    LOG_D("\r\n");
    LOG_D("ram_base:0x%x\r\n", &Image$$RW_IRAM1$$Base);
    LOG_D("ram_length:0x%x\r\n", &Image$$RW_IRAM1$$Length);
    LOG_D("ram_limit:0x%x\r\n\r\n", &Image$$RW_IRAM1$$Limit);
    
    LOG_D("rom_base:0x%x\r\n", &Image$$ER_IROM1$$Base);
    LOG_D("rom_length:0x%x\r\n", &Image$$ER_IROM1$$Length);
    LOG_D("rom_limit:0x%x 0x%x\r\n\r\n", &Image$$ER_IROM1$$Limit, *(int*)&Image$$ER_IROM1$$Limit);
    #endif

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

static void init(Device* dev) {
    platformSysInit();
    sdkSysInit();
    sdkLogSetLevel(4);
    sdkEmvDebug(0);
    libPropertiesLoad();
    
    dev->module.wifi = sdkSysIsDeviceExist(SYS_DEVICE_WIFI);
    dev->module.gprs = sdkSysIsDeviceExist(SYS_DEVICE_WIRELESS);
    dev->module.printer = sdkSysIsDeviceExist(SYS_DEVICE_PRINTER);
    dev->module.audio = sdkSysIsDeviceExist(SYS_DEVICE_AUDIO);
    dev->module.scanner = sdkSysIsDeviceExist(SYS_DEVICE_CAMERA);
    dev->module.bt = sdkSysIsDeviceExist(SYS_DEVICE_BLUETOOTH);
}

static void getTick(Device* dev) {
    return sdkSysGetTicks();
}

static unsigned int getMemory(Device* dev, unsigned int size) {
    return sdkSysGetMem(size);
}

static void freeMemory(Device* dev, void *mem) {
    sdkSysFreeMem(mem);
}

static unsigned int flushDisplay(Device* dev, int32_t x0, int32_t x1, int32_t y0, int32_t y1, uint32_t color) {
    strRect rect = {
        .m_x0 = x0,
        .m_x1 = x1,
        .m_y0 = y0,
        .m_y1 = y1
    };
    strPicture pic = {
        .m_width = x1 - x0 + 1,
        .m_height = y1 - y0 + 1,
        .m_pic = color
    };
    ddi_lcd_show_picture(&rect, &pic);
}

static void logOut(Device* dev, const char *data, size_t len, void *udata) {
    sdkLogOut(data);
}

static DateTime *getDateTime(Device* dev) {
    uint8_t dt[6 + 6 + 1];
    memset(dt, 0, sizeof(dt));
    sdkSysGetRtcTime(dt);
    memcpy(dateTime.date, dt, 6);
    memcpy(dateTime.time, dt + 6, 6);
    return &dateTime;
}

static BatteryStat* getBatteryStatus(Device *dev) {
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

static void sysSleep(Device *dev, uint32_t mili) {
    sdkSysSleep(mili);
}

static void reboot(Device *dev) {
    sdkSysDeviceReboot();
}

static void powerOff(Device *dev) {
    sdkSysDevicePowerOff();
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
}

#endif