#include "states.h"
#include "logger.h"
#include "event.h"
#include "dev/dev.h"
#include "display.h"
#include "keypad/keypad.h"
#include "magReader/magReader.h"
#include "wifi/wifi.h"
#include "statusBar/statusBar.h"
#include "eventloop.h"
#include "timer.h"

static lv_obj_t *startUpPage;
static lv_obj_t *label;

STATE_DEF_ENTER() {
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
}

STATE_DEF_EXIT() {

}

STATE_DEF_HANDLE(TimeOutEvent) {

}

STATE_DEF_HANDLE(KeypadEvent) {
        LOG_TRACE("event successfully is reached. key = %d", ev->key);
    if (ev->key == KEY_ESC)
        OOP_CALL(getDevice(), powerOff);
}

OOP_CTOR(Startup, State *parent, const char *name) {
    LOG_TRACE("Constructing Startup ...");
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER();
    self->base.vtable.exit = STATE_EXIT();
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleWifi = STATE_HANDLE(WifiEvent);
}