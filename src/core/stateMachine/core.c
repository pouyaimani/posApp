#include "core.h"
#include "state.h"
#include "event.h"
#include <stdlib.h>
#include "logger.h"
#include "hal/dev/dev.h"
#include "common.h"

Core __core;
Device *dev;

static void init(State *initial)
{
    LOG_TRACE("SM: initialization starts.");
    RETURN_IF_NULL(initial, ;);
    __core.current = initial;
    __core.current->inner = STATE_ENTRY;
}

static void raiseEvent(Event *ev)
{
    LOG_TRACE("SM: Event is going to raise.");
    RETURN_IF_NULL(ev, ;);
    if (!ev->target) {
        LOG_TRACE("SM: Event target is null, setting it to current state.");
        ev->target = __core.current;
    }
    __core.queue[__core.qsize++] = ev;
    LOG_TRACE("SM: raising event fnished.");
}

static void goTo(State *next)
{
    RETURN_IF_NULL(next, ;);
    __core.current->inner = STATE_EXIT;
    __core.next = next;
}

static void goToSub(State *next)
{
    RETURN_IF_NULL(next, ;);
    __core.current->inner = STATE_SUBSTATE;
    __core.next = next;
}

static void freeQ() {
    for (size_t i = 0; i < __core.qsize; ) {
        Event *ev = __core.queue[i];
        if (ev->target == __core.current) {
            OOP_CALL(dev, freeMemory, ev);
            __core.queue[i] = __core.queue[--__core.qsize];
        } else {
            i++;
        }
    }
}

static void runCycle()
{
    State *s = __core.current;

    switch (s->inner) {
    case STATE_ENTRY:
        LOG_TRACE("SM: on entry to %s %s", s->name, " state.");
        s->inner = STATE_EVENT;
        OOP_CALL(s, enter);
        break;

    case STATE_EVENT:
        for (size_t i = 0; i < __core.qsize; ) {
            Event *ev = __core.queue[i];
            if (ev->target == s) {
                LOG_TRACE("SM: Event came to %s %s", s->name, " state.");
                OOP_CALL(ev, dispatchTo, s);
                OOP_CALL(dev, freeMemory, ev);
                __core.queue[i] = __core.queue[--__core.qsize];
                LOG_TRACE("SM: Event dispatched to %s %s", s->name, " state.");
            } else {
                i++;
            }
        }
        break;

    case STATE_EXIT:
        LOG_TRACE("SM: on exit from %s %s", s->name, " state.");
        OOP_CALL(s, exit);
        freeQ();
        s->inner = STATE_ENTRY;
        __core.current = __core.next;
        break;

    case STATE_SUBSTATE:
        LOG_TRACE("SM: on substate to ", s->name, " state.");
        OOP_CALL(s, exit);
        freeQ();
        s->inner = STATE_EVENT;
        __core.current = __core.next;
        break;
    }
}

static void exec()
{
    LOG_TRACE("SM: starts execution.");
    while (1) {
        runCycle();
        for (size_t i = 0; i < __core.cbSize ; ++i) {
            __core.callbacks[i]();
        }
    }
}

void registerCallback(CoreCallback cb)
{
    LOG_TRACE("SM: new callback is registring, cb adress = %d", cb);
    if (__core.cbSize < 16) {
        LOG_TRACE("SM: new callback is registered.");
        __core.callbacks[__core.cbSize++] = cb;
    } else {
        LOG_FATAL("SM: registering callback failed: queue is full.");
    }
}

OOP_CTOR(Core) {
    LOG_TRACE("SM: core constructor, core address = %d", &__core);
    __core.init = init;
    __core.exec = exec;
    __core.goTo = goTo;
    __core.goToSub = goToSub;
    __core.raiseEvent = raiseEvent;
    __core.runCycle = runCycle;
    __core.registerCallback = registerCallback;
    __core.current = NULL;
    __core.next = NULL;
    __core.qsize = 0;
    __core.cbSize = 0;
    dev = getDevice();
}

Core *getSmCore(void)
{
    CALL_ONCE(
        OOP_CALL_CTOR(Core, &__core);
    );
    return &__core;
}