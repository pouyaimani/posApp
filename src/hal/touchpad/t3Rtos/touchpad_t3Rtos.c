#ifdef DEVICE_TRENDITT3RTOS

#include "touchpad_t3Rtos.h"
#include "posplatform.h"
#include <sdkTp.h>

static void init(Touchpad* tp) {
    sdkTPOpen();
}

static void readTp(Touchpad* tp) {
    static TpEvent ev = {0};
    tp->x = 0;
    tp->y = 0;

    sdkTPRead(&ev);

    if(ev.type == EVENT_TOUCH_HOLD || ev.type == EVENT_TOUCH_BEGIN) {
        tp->x = ev.x;
        tp->y = ev.y;
        tp->state = TP_STATE_PRESS;
    }
    else {
        tp->state = TP_STATE_RELEASE;
    }
}

OOP_CTOR(TouchpadT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.read = readTp;
}

#endif