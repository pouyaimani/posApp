#ifdef DEVICE_TRENDITT3RTOS

#include "dev_t3Rtos.h"
#include "posplatform.h"
#include "ddi.h"
#include "ddiapi.h"

static void loadIng(void)
{
    libDispPrompt(("Initializing..."), 1);
}

static void appInit(void)
{
    platformSysInit();

    sdkSysInit();
    
    sdkLogSetLevel(4);
    sdkEmvDebug(0);

    libDeviceInit();
    libPropertiesLoad();
    libDispInit();
    libStBarInit();
    loadIng();
    libCommInit();
    recordInit();
    initSocketAsyncControl();
    loadAidCapk();
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
    self->base.name = name;
}

#endif