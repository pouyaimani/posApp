#include "core.h"
#include "state.h"
#include "event.h"
#include <stdlib.h>

static Core core;

static void init(State *initial)
{
    RETURN_IF_NULL(initial);
    core.current = initial;
    core.current->inner = STATE_ENTRY;
}

static void raiseEvent(Event *ev)
{
    RETURN_IF_NULL(ev);
    if (!ev->target)
        ev->target = core.current;
    core.queue[core.qsize++] = ev;
}

static void goTo(State *next)
{
    RETURN_IF_NULL(next);
    core.current->inner = STATE_EXIT;
    core.next = next;
}

static void runCycle()
{
    State *s = core.current;

    switch (s->inner) {
    case STATE_ENTRY:
        s->inner = STATE_EVENT;
        OOP_CALL(s, enter);
        break;

    case STATE_EVENT:
        for (size_t i = 0; i < core.qsize; ) {
            Event *ev = core.queue[i];
            if (ev->target == s) {
                OOP_CALL(ev, dispatchTo, s);
                free(ev);
                core.queue[i] = core.queue[--core.qsize];
            } else {
                i++;
            }
        }
        break;

    case STATE_EXIT:
        OOP_CALL(s, exit);
        for (size_t i = 0; i < core.qsize; ) {
            Event *ev = core.queue[i];
            if (ev->target == s) {
                free(ev);
                core.queue[i] = core.queue[--core.qsize];
            } else {
                i++;
            }
        }
        s->inner = STATE_ENTRY;
        core.current = core.next;
        break;
    }
}

static void exec()
{
    while (1) {
        runCycle();
        for (size_t i = 0; i < core.cbSize ; ++i) {
            core.callbacks[i]();
        }
    }
}

void registerCallback(CoreCallback cb)
{
    if (core.cbSize < 16) {
        core.callbacks[core.cbSize++] = cb;
    }
}

OOP_CTOR(Core) {
    core.init = init;
    core.exec = exec;
    core.goTo = goTo;
    core.raiseEvent = raiseEvent;
    core.runCycle = runCycle;
    core.current = NULL;
    core.next = NULL;
    core.qsize = 0;
    core.cbSize = 0;
}

Core *getSmCore(void)
{
    CALL_ONCE(
        Core_ctor(&core);
    );
    return &core;
}