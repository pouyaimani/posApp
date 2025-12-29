#include "keypad.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/keypad_t3Rtos.h"
#endif

#define KEYPAD_REGISTER(type)       \
    static type obj;                \
    Keypad_ctor(keypad);            \
    type##_ctor(&obj);              \
    keypad = (Keypad*)&obj;

static Keypad *keypad;

static Key_t getKey() {
    Key_t pressedKey = keypad->key;
    keypad->key = KEY_NONE;
    return pressedKey;
}

static bool isPressed() {
    return keypad->key != KEY_NONE;
}

void Keypad_ctor(Keypad* self) {
    self->key = KEY_NONE;
    self->vtable.getKey = getKey;
    self->vtable.isPressed = isPressed;
}

Keypad *getKeypad() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    KEYPAD_REGISTER(KeypadT3Rtos)
#else
#error Deivce keypad is undefined. Make sure correct device is chosen and its keypad driver is developed.
#endif
    );
    return keypad;
}