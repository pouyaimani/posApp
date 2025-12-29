#include "keypad.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/keypad_t3Rtos.h"
#endif

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
    self->getKey = getKey;
    self->isPressed = isPressed;
}

Keypad *getKeypad() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        static KeypadT3Rtos t3Rtos;
        KeypadT3Rtos_ctor(&t3Rtos);
        keypad = (Keypad*)&t3Rtos;
#else
#error Deivce keypad is undefined. Make sure correct device is chosen and its keypad driver is developed.
#endif
    );
    Keypad_ctor(keypad);
    return keypad;
}