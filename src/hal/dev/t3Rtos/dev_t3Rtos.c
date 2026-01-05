#ifdef DEVICE_TRENDITT3RTOS

#include "dev_t3Rtos.h"
#include "posplatform.h"
#include "sdkSys.h"
#include "appVersion.h"
#include "LibProperties.h"
#include "sdkUtils.h"

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

void T3Rtos_ctor(T3Rtos* self, const char* name) {
    self->base.vtable.init = init;
    self->base.vtable.getTick = getTick;
    self->base.vtable.getMemory = getMemory;
    self->base.vtable.flushDisplay = flushDisplay;
    self->base.vtable.freeMemory = freeMemory;
    self->base.name = name;
}

#endif