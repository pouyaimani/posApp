#include "statusBar.h"
#include "dev/dev.h"
#include "timer.h"
#include "logger.h"

static StatusBar *__statusBar;
static Timer *timer;

static void update() {
    BatteryStat *bat = OOP_CALL(getDevice(), getBatteryStatus);
    LOG_TRACE("battery percent %d", bat->percent);
    if (bat->isChanrging) {
        LOG_TRACE("battery is chargine ...");
    }
}

OOP_CTOR(StatusBar) {
    timer = TIMER_CREATE(update, SECS(10), false);
}

void createStatusBar() {
    CALL_ONCE(
        __statusBar = (StatusBar*)GET_MEM(sizeof(StatusBar));
        OOP_CALL_CTOR(StatusBar, __statusBar);
    );
    return __statusBar;
}