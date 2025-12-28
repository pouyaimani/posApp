#ifdef DEVICE_TRENDITT3RTOS

#include "dev_t3Rtos.h"
#include "posplatform.h"

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

static void T3Rtos_init(Device* dev) {
    appInit();
}

static void T3Rtos_getTick(Device* dev) {
    return sdkSysGetTicks();
}

static unsigned int T3Rtos_getMemory(Device* dev, unsigned int size) {
    return sdkSysGetMem(size);
}

static const DeviceVTable t3Rtos_vtable = {
    .init = T3Rtos_init,
    .getTick = T3Rtos_getTick,
    .getMemory = T3Rtos_getMemory
};

void T3Rtos_ctor(T3Rtos* self, const char* name) {
    self->base.vtable = &t3Rtos_vtable;
    self->base.name = name;
}

#endif