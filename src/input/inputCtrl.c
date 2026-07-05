#include "inputCtrl.h"

extern void inputKeypadInit(InputController* c);

InputResult inputUpdate(InputController* c, InputEvent* ev) {
    return c->cb->handle(ev, c->cfg);
}

void inputInit(InputController* c) { inputKeypadInit(c); }