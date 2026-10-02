#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include "event.h"

typedef enum {
    INPUT_EV_NONE,
    INPUT_EV_KEYPAD,
    INPUT_EV_PED,
    INPUT_EV_TIMEOUT
} InputEventType;

typedef struct {
    InputEventType type;
    union {
        KeypadEvent keypad;
        PedEvent    ped;
    };

} InputEvent;

#endif