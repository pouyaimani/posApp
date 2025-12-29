#ifdef DEVICE_TRENDITT3RTOS

#include "keypad_t3Rtos.h"
#include "posplatform.h"
#include <sdkKey.h>

static void KeypadT3Rtos_init(KeypadT3Rtos* keypad) {
}

static Key_t sdk_key_to_key(SDK_KEY_VALUE sdk_key)
{
    switch (sdk_key) {
    case KEY_VALUE_0: return KEY_0;
    case KEY_VALUE_1: return KEY_1;
    case KEY_VALUE_2: return KEY_2;
    case KEY_VALUE_3: return KEY_3;
    case KEY_VALUE_4: return KEY_4;
    case KEY_VALUE_5: return KEY_5;
    case KEY_VALUE_6: return KEY_6;
    case KEY_VALUE_7: return KEY_7;
    case KEY_VALUE_8: return KEY_8;
    case KEY_VALUE_9: return KEY_9;

    case KEY_VALUE_ENTER:    return KEY_ENTER;
    case KEY_VALUE_ESC:      return KEY_ESC;
    case KEY_VALUE_UP:       return KEY_UP;
    case KEY_VALUE_DOWN:     return KEY_DOWN;
    case KEY_VALUE_CLEAR:    return KEY_CLEAR;
    case KEY_VALUE_ALPHA:    return KEY_ALPHA;
    case KEY_VALUE_FUNCTION: return KEY_FUNCTION;
    case KEY_VALUE_POWER:    return KEY_POWER;

    default:
        return KEY_NONE;
    }
}

static void KeypadT3Rtos_read(KeypadT3Rtos* keypad) {
    if (keypad->base.key == KEY_NONE) {
        keypad->base.key = sdk_key_to_key(sdkKeyGet());
    }
}

void KeypadT3Rtos_ctor(KeypadT3Rtos* self) {
    self->base.vtable.init = KeypadT3Rtos_init;
    self->base.vtable.read = KeypadT3Rtos_read;
}

#endif