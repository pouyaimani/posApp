#ifdef DEVICE_TRENDITT3RTOS

#include "dev_t3Rtos.h"
#include "posplatform.h"
#include "sdkSys.h"
#include "appVersion.h"
#include "LibProperties.h"

#define DEVICE_MACHINE_ID  "33"
#define USER_DATA_ROOT_DIR "/mtd0/"
#define APP_DIR             USER_DATA_ROOT_DIR APPID "/"
#define IMG_PATH "img/"

static bool gIsWifiExist = false;
static bool gIsGprsExist = false;
static bool gIsPrintExist = false;
static bool gIsAudioExist = false;
static bool gIsScannerExist = false;
static bool gIsMoNoLcd = false;
static bool gIsDeviceDebug = false;
static bool gIsHalSupportSwitchCell = false;
static bool gIsBTExist = false;
static bool gIsSupportHwStatusBar = false;

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

static void loadIng(void)
{
    libDispPrompt(("Initializing..."), 1);
}

static void platformSysInit(void)
{
    initRam();

    sdkEmvSetAppDir(APP_DIR);
}

static void deviceInit()
{
    gIsWifiExist = sdkSysIsDeviceExist(SYS_DEVICE_WIFI);
    gIsGprsExist = sdkSysIsDeviceExist(SYS_DEVICE_WIRELESS);
    gIsPrintExist = sdkSysIsDeviceExist(SYS_DEVICE_PRINTER);
    gIsAudioExist = sdkSysIsDeviceExist(SYS_DEVICE_AUDIO);
    gIsScannerExist = sdkSysIsDeviceExist(SYS_DEVICE_CAMERA);
    gIsBTExist = sdkSysIsDeviceExist(SYS_DEVICE_BLUETOOTH);
    gIsSupportHwStatusBar = false;
    gIsMoNoLcd = false;

#if 0
    LOG_D("gIsWifiExist = %d\r\n", sdkSysIsDeviceExist(SYS_DEVICE_WIFI));
    LOG_D("gIsGprsExist = %d\r\n", sdkSysIsDeviceExist(SYS_DEVICE_WIRELESS));
    LOG_D("gIsPrintExist = %d\r\n", sdkSysIsDeviceExist(SYS_DEVICE_PRINTER));
    LOG_D("gIsAudioExist = %d\r\n", sdkSysIsDeviceExist(SYS_DEVICE_AUDIO));
    LOG_D("gIsScannerExist = %d\r\n", sdkSysIsDeviceExist(SYS_DEVICE_CAMERA));
    LOG_D("gIsBTExist = %d\r\n", sdkSysIsDeviceExist(SYS_DEVICE_BLUETOOTH));
    LOG_D("gIsSupportHwStatusBar = %d\r\n", false);
    LOG_D("gIsMoNoLcd = %d\r\n", false);
#endif
}

static void appInit(void)
{
    platformSysInit();

    sdkSysInit();
    
    sdkLogSetLevel(4);
    sdkEmvDebug(0);

    deviceInit();
    // libPropertiesLoad();
    // libDispInit();
    // libStBarInit();
    // loadIng();
    // libCommInit();
    // recordInit();
    // initSocketAsyncControl();
    // loadAidCapk();
}

static void init(Device* dev) {
    appInit();
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