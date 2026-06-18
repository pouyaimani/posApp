#ifndef EVENTLOOP_H_
#define EVENTLOOP_H_

#include "oop.h"

typedef void (*EventChecker)(void);

/* Configuration */
#define EVENTLOOP_MAX_CHECKERS 8

typedef enum EvLoopErr_t {
    EV_LOOP_OK,
    EV_LOOP_FULL,
    EV_LOOP_CHECKER_NOT_FOUND
} EvLoopErr_t;

// NOTE: Do not create object of Eventloop yourself, instead use getEventloop()
// function
OOP_DECLARE_CLASS(Eventloop)

OOP_CLASS(Eventloop) {
    EventChecker checkers[EVENTLOOP_MAX_CHECKERS];
    size_t       checkersCnt;
    OOP_METHOD(void, runCycle);
    OOP_METHOD(EvLoopErr_t, registerChecker, EventChecker);
    OOP_METHOD(EvLoopErr_t, unregisterChecker, EventChecker);
    OOP_METHOD(EvLoopErr_t, unregisterAll);
};

OOP_CTOR(Eventloop);

Eventloop* getEventloop(void);

#endif