#include "event.h"
#include "state.h"
#include "core.h"
#include <stdlib.h>
#include "hal/dev/dev.h"

/* ================= Event base ================= */
static void dispatch(Event *self, State *state)
{
    self->target = state;
    getSmCore()->raiseEvent(self);
}

void Event_ctor(Event* self) {
    self->vtable.dispatch = dispatch;
}

Event **createEvent(SmEventType_t type) {
    Event *event;
    Device *dev = getDevice();
    switch (type) {
    case SM_EVENT_TIME_OUT:
        event = (TimeOutEvent*)OOP_CALL(dev, getMemory, sizeof(TimeOutEvent));
        break;
    case SM_EVENT_KEYPAD:
        event = (KeypadEvent*)OOP_CALL(dev, getMemory, sizeof(KeypadEvent));
        break;
    default:
        break;
    }
    Event_ctor(event);
    return &event;
}

/* ================= TimeOut ================= */

static void TimeOut_dispatchTo(Event *self, State *state)
{
    OOP_CALL(state, handleTimeout, self);
}

OOP_CTOR(TimeOutEvent)
{
    self->base.vtable.dispatchTo = TimeOut_dispatchTo;
}

/* ================= Keypad ================= */

static void Keypad_dispatchTo(Event *self, State *state)
{
    OOP_CALL(state, handleKeypad, self);
}

OOP_CTOR(KeypadEvent, KeypadKey key, const char *str)
{
    self->base.vtable.dispatchTo = Keypad_dispatchTo;
}