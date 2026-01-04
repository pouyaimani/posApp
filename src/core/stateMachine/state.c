#include "state.h"
#include "core.h"
#include <stdio.h>

/* defaults */

static void default_enter(State *s)
{
    //TODO -> develop log system
    printf("%s enter not defined\n", s->name);
}

static void default_exit(State *s)
{
    printf("%s exit not defined\n", s->name);
}

static void goTo(State *s, State *nextState)
{
    getSmCore()->goTo(nextState);
}

static void default_timeout(State *s, TimeOutEvent *ev)
{
    (void)ev;
    printf("%s timeout not handled\n", s->name);
}

static void default_keypad(State *s, KeypadEvent *ev)
{
    (void)ev;
    printf("%s keypad not handled\n", s->name);
}

OOP_CTOR(State, State *parent, const char *name)
{
    self->vtable.enter = default_enter;
    self->vtable.exit = default_exit;
    self->vtable.handleTimeout = default_timeout;
    self->vtable.handleKeypad = default_keypad;
    self->parent = parent;
    self->name = name;
    self->inner = STATE_ENTRY;
}