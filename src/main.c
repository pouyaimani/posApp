#include "hal/dev/dev.h"
#include "core/stateMachine/core.h"
#include "states/states.h"
#include "logger.h"

//RTOS entry point
void appMain(void)
{
    Device *dev = getDevice();
    OOP_CALL(dev, init);
#if USE_LOG
    logger()->init(&(LogConfig_t)  {
        .writer = {
            .write = dev->vtable.logOut,
            .udata = NULL
        },
        .getDateTime = dev->vtable.getDateTime
    });
#endif
    Core *core = getSmCore();
    core->init(getState(STATE_ID_START_UP));
    core->exec();
}

#ifdef DEVICE_USES_LINUX

int main() {
    appMain();
}

#endif