#include "keypad.h"

#include "core/stateMachine/event.h"
#include "core/eventloop/eventloop.h"
#include "logger.h"

Keypad *__keypad;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/keypad_t3Rtos.h"

static void constructT3Rtos() {
    static KeypadT3Rtos obj;
    __keypad = (Keypad *)&obj;
    OOP_CALL_CTOR(Keypad, __keypad);
    OOP_CALL_CTOR(KeypadT3Rtos, &obj);
}

#endif

static Key_t getKey(Keypad* self) {
    Key_t pressedKey = self->key;
    self->key = KEY_NONE;
    return pressedKey;
}

static bool isPressed(Keypad* self) {
    return self->key != KEY_NONE;
}

static void ioRead() {
    OOP_CALL(__keypad, readKey);
    if (isPressed(__keypad)) {
        KeypadEvent *ev = (KeypadEvent**)createEvent(SM_EVENT_KEYPAD);
        ev->key = getKey(__keypad);
        LOG_TRACE("Keypad: key pressed: %d", ev->key);
        ev->keyStr = "";
        DISPATCH_EVENT(ev);
    }
}

OOP_CTOR(Keypad) {
    LOG_TRACE("Constructing keypad ...");
    self->key = KEY_NONE;
    self->vtable.getKey = getKey;
    self->vtable.isPressed = isPressed;
    getEventloop()->registerChecker(ioRead);
}

Keypad *getKeypad() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce keypad is undefined. Make sure correct device is chosen and its keypad driver is developed.
#endif
    );
    return __keypad;
}