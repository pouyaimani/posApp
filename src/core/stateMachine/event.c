#include "event.h"
#include "state.h"
#include "core.h"
#include <stdlib.h>
#include "hal/dev/dev.h"
#include "hal/keypad/keypad.h"

#define CREATE_EVENT(type, event)                               \
    do {                                                        \
        type## *ev = OOP_CALL(dev, getMemory, sizeof(type##));  \
        event = (Event*)ev;                                     \
        OOP_CALL_CTOR(Event, event);                            \
        OOP_CALL_CTOR(type##, ev);                              \
    } while(0)

/* ================= Event base ================= */
static void dispatch(Event *self, State *state)
{
    self->target = state;
    getSmCore()->raiseEvent(self);
}

OOP_CTOR(Event) {
    self->vtable.dispatch = dispatch;
}

Event *createEvent(SmEventType_t type) {
    Event *event;
    Device *dev = getDevice();
    switch (type) {
    case SM_EVENT_TIME_OUT:
        CREATE_EVENT(TimeOutEvent, event);
        break;
    case SM_EVENT_KEYPAD:
        CREATE_EVENT(KeypadEvent, event);
        break;
    case SM_EVENT_MAG:
        CREATE_EVENT(MagEvent, event);
        break;
    default:
        break;
    }
    return event;
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

OOP_CTOR(KeypadEvent)
{
    self->base.vtable.dispatchTo = Keypad_dispatchTo;
}

/* ================= MagReader ================= */

static void mag_dispatchTo(Event *self, State *state)
{
    OOP_CALL(state, handleMag, self);
}

OOP_CTOR(MagEvent)
{
    self->base.vtable.dispatchTo = mag_dispatchTo;
}