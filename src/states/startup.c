#include "states.h"
#include "logger.h"
#include "event.h"
#include "dev/dev.h"
#include "display.h"
#include "keypad/keypad.h"
#include "magReader/magReader.h"
#include "eventloop.h"
#include "timer.h"
#include "wifi/wifi.h"

static lv_obj_t *startUpPage;
static lv_obj_t *label;

STATE_DEF_ENTER(Startup) {
    KEYPAD_INIT();
    MAG_INIT();
    WIFI_INIT();
    Display *disp = getDisplay();
    disp->init();
    Core *core = getSmCore();
    core->registerCallback(disp->update);
    core->registerCallback(getEventloop()->runCycle);
    core->registerCallback(getTimerHanlder()->runCycle);
    createStatusBar();
    SM_GOTO(getState(STATE_ID_IDLE));
}

STATE_DEF_EXIT(Startup) {

}

STATE_DEF_HANDLE(TimeOutEvent) {

}

OOP_CTOR(Startup, State *parent, const char *name) {
    LOG_TRACE("Constructing Startup ...");
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Startup);
    self->base.vtable.exit = STATE_EXIT(Startup);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
}