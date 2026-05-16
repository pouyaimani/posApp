#include "touchpad.h"
#include "core/stateMachine/event.h"
#include "core/eventloop/eventloop.h"
#include "logger.h"

Touchpad *__touch;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/touchpad_t3Rtos.h"

static void constructT3Rtos() {
    static TouchpadT3Rtos obj;
    __touch = (Touchpad *)&obj;
    OOP_CALL_CTOR(Touchpad, __touch);
    OOP_CALL_CTOR(TouchpadT3Rtos, &obj);
}

#endif

OOP_CTOR(Touchpad) {
    LOG_TRACE("Constructing touchpad ...");
}

Touchpad *touchpad() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce keypad is undefined. Make sure correct device is chosen and its keypad driver is developed.
#endif
    );
    return __touch;
}