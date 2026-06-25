#include "core.h"
#include "state.h"
#include "event.h"
#include <stdlib.h>
#include "logger.h"
#include "sys/sys.h"
#include "common.h"

Core __core;

static void init(State* initial) {
    STM_LOG("SM: initialization starts.");
    RETURN_IF_NULL(initial, ;);
    __core.current        = initial;
    __core.current->inner = STATE_ENTRY;
}

static void raiseEvent(Event* ev) {
    STM_LOG("SM: Event is going to raise.");
    RETURN_IF_NULL(ev, ;);
    if (!ev->target) {
        STM_LOG("SM: Event target is null, setting it to current state.");
        ev->target = __core.current;
    }
    __core.queue[__core.qsize++] = ev;
    STM_LOG("SM: raising event fnished.");
}

static void goTo(State* next) {
    RETURN_IF_NULL(next, ;);
    __core.current->inner = STATE_EXIT;
    __core.next           = next;
}

static void goToSub(State* next) {
    RETURN_IF_NULL(next, ;);
    __core.current->inner = STATE_SUBSTATE;
    __core.next           = next;
}

static void freeQ() {
    for (size_t i = 0; i < __core.qsize;) {
        Event* ev = __core.queue[i];
        if (ev->target == __core.current) {
            OOP_CALL(sys(), freeMemory, ev);
            __core.queue[i] = __core.queue[--__core.qsize];
        } else {
            i++;
        }
    }
}

static void runCycle() {
    State* s = __core.current;

    switch (s->inner) {
    case STATE_ENTRY:
        STM_LOG("SM: on entry to %s %s", s->name, " state.");
        s->inner = STATE_EVENT;
        OOP_CALL(s, enter);
        break;

    case STATE_EVENT:
        for (size_t i = 0; i < __core.qsize;) {
            Event* ev = __core.queue[i];
            if (ev->target == s) {
                STM_LOG("SM: Event came to %s %s", s->name, " state.");
                OOP_CALL(ev, dispatchTo, s);
                OOP_CALL(sys(), freeMemory, ev);
                __core.queue[i] = __core.queue[--__core.qsize];
                STM_LOG("SM: Event dispatched to %s %s", s->name, " state.");
            } else {
                i++;
            }
        }
        break;

    case STATE_EXIT:
        STM_LOG("SM: on exit from %s %s", s->name, " state.");
        OOP_CALL(s, exit);
        freeQ();
        s->inner       = STATE_ENTRY;
        __core.current = __core.next;
        break;

    case STATE_SUBSTATE:
        STM_LOG("SM: on substate to ", s->name, " state.");
        OOP_CALL(s, exit);
        freeQ();
        s->inner       = STATE_EVENT;
        __core.current = __core.next;
        break;
    }
}

static void exec() {
    STM_LOG("SM: starts execution.");
    while (1) {
        runCycle();
        for (size_t i = 0; i < __core.cbSize; ++i) {
            __core.callbacks[i]();
        }
    }
}

void registerCallback(CoreCallback cb) {
    STM_LOG("SM: new callback is registring, cb adress = %d", cb);
    if (__core.cbSize < 16) {
        STM_LOG("SM: new callback is registered.");
        __core.callbacks[__core.cbSize++] = cb;
    } else {
        LOG_FATAL("SM: registering callback failed: queue is full.");
    }
}

OOP_CTOR(Core) {
    STM_LOG("SM: core constructor, core address = %d", &__core);
    __core.init             = init;
    __core.exec             = exec;
    __core.goTo             = goTo;
    __core.goToSub          = goToSub;
    __core.raiseEvent       = raiseEvent;
    __core.runCycle         = runCycle;
    __core.registerCallback = registerCallback;
    __core.current          = NULL;
    __core.next             = NULL;
    __core.qsize            = 0;
    __core.cbSize           = 0;
}

Core* smCore(void) {
    CALL_ONCE(OOP_CALL_CTOR(Core, &__core););
    return &__core;
}