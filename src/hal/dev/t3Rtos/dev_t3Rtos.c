#ifdef DEVICE_TRENDITT3RTOS

#include "dev_t3Rtos.h"
#include "posplatform.h"
// #include "LibGlobal.h"

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

static const DeviceVTable t3Rtos_vtable = {
    .init = T3Rtos_init
};

void T3Rtos_ctor(T3Rtos* self, const char* name) {
    self->base.vtable = &t3Rtos_vtable;
    self->base.name = name;
}

#endif