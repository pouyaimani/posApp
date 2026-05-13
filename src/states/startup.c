#include "states.h"
#include "logger.h"
#include "event.h"
#include "sys/sys.h"
#include "display.h"
#include "keypad/keypad.h"
#include "touchpad/touchpad.h"
#include "magReader/magReader.h"
#include "eventloop.h"
#include "timer.h"
#include "wifi/wifi.h"
#include "storage/storage.h"
#include "network/network.h"
#include "settings/settings.h"
#include "record/shiftRecs.h"

static lv_obj_t *startUpPage;
static lv_obj_t *label;

STATE_DEF_ENTER(Startup) {
    KEYPAD_INIT();
    MAG_INIT();
    WIFI_INIT();
    TOUCH_INIT();
    // settings()->reset();
    settings()->load();
    Display *disp = getDisplay();
    disp->init();
    Core *core = getSmCore();
    core->registerCallback(disp->update);
    core->registerCallback(getEventloop()->runCycle);
    core->registerCallback(getTimerHanlder()->runCycle);

    // Network setitings
    network()->init();

    shifts()->init();

    LOG_DEBUG("device voulme = %d", settings()->terminal.devVolume);
    OOP_CALL(sys(), setVolume, settings()->terminal.devVolume);
    OOP_CALL(sys(), setBrightness, settings()->terminal.brightness);
    statusBar();
    GOTO_IDLE();
}

STATE_DEF_EXIT(Startup) {

}

STATE_DEF_HANDLE(Startup, TimeOutEvent) {

}

OOP_CTOR(Startup, State *parent, const char *name) {
    LOG_TRACE("Constructing Startup ...");
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Startup);
    self->base.vtable.exit = STATE_EXIT(Startup);
    self->base.vtable.handleTimeout = STATE_HANDLE(Startup, TimeOutEvent);
}