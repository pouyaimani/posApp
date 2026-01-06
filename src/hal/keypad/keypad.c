#include "keypad.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/keypad_t3Rtos.h"
#endif

#include "core/stateMachine/event.h"
#include "core/eventloop/eventloop.h"

static Keypad *keypad;

static Key_t getKey() {
    Key_t pressedKey = keypad->key;
    keypad->key = KEY_NONE;
    return pressedKey;
}

static bool isPressed(Keypad* self) {
    return keypad->key != KEY_NONE;
}

static void ioRead(Keypad* self) {
    OOP_CALL(keypad, readKey);
    if (isPressed(self)) {
        KeypadEvent **ev = (KeypadEvent**)createEvent(SM_EVENT_KEYPAD);
        (*ev)->key = getKey();
        (*ev)->keyStr = "";
        DISPATCH_EVENT(*ev);
    }
}

void Keypad_ctor(Keypad* self) {
    self->key = KEY_NONE;
    self->vtable.getKey = getKey;
    self->vtable.isPressed = isPressed;
    self->vtable.ioRead = ioRead;
    getEventloop()->registerChecker(self->vtable.ioRead);
}

Keypad *getKeypad() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    DEVICE_REGISTER(Keypad, KeypadT3Rtos, keypad)
#else
#error Deivce keypad is undefined. Make sure correct device is chosen and its keypad driver is developed.
#endif
    );
    return keypad;
}