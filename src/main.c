#include "hal/dev/dev.h"
#include "display/display.h"
#include "core/eventloop/eventloop.h"
#include "core/stateMachine/core.h"
#include "states/states.h"

int main() {
    Device *dev = getDevice();
    OOP_CALL(dev, init);
    Display *disp = getDisplay();
    disp->init();
    Core *core = getSmCore();
    core->init(getState(STATE_ID_START_UP));
    core->registerCallback(disp->update);
    core->registerCallback(getEventloop()->runCycle);
    core->exec();
}
