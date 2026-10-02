#ifndef INPUT_PROVIDER_H
#define INPUT_PROVIDER_H

#include "inputEvent.h"

typedef struct InputProvider {

    bool (*start)(void);

    bool (*poll)(struct InputProvider* self, InputEvent* ev);

    void (*stop)(void);

} InputProvider;

#endif