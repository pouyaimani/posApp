#include "sys/sys.h"
#include "core/stateMachine/core.h"
#include "states/states.h"
#include "logger.h"

//RTOS entry point
void appMain(void)
{
    OOP_CALL(sys(), init);
#if USE_LOG
    logger()->init(&(LogConfig_t)  {
        .writer = {
            .write = sys()->vtable.logOut,
            .udata = NULL
        },
        .getDateTime = sys()->vtable.getDateTime
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