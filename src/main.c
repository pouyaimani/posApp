#include "hal/dev/dev.h"
#include "display/display.h"
#include "core/eventloop/eventloop.h"
#include "core/stateMachine/core.h"
#include "hal/keypad/keypad.h"
#include "states/states.h"
#include "logger.h"

//RTOS entry point
void appMain(void)
{
    Device *dev = getDevice();
    OOP_CALL(dev, init);
#if USE_LOG
    initLogger(&(LogConfig_t)  {
        .writer = {
            .write = dev->vtable.logOut,
            .udata = NULL
        },
        .getDateTime = dev->vtable.getDateTime
    });
#endif
    Display *disp = getDisplay();
    disp->init();
    Core *core = getSmCore();
    core->init(getState(STATE_ID_START_UP));
    core->registerCallback(disp->update);
    core->registerCallback(getEventloop()->runCycle);
    core->exec();
}

#ifdef DEVICE_USES_LINUX

int main() {
    appMain();
}

#endif