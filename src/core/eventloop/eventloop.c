#include "eventloop.h"

Eventloop __loop;

static void runCycle() {
    for (size_t i = 0; i < __loop.checkersCnt; ++i) {
        __loop.checkers[i]();
    }
}

static EvLoopErr_t registerChecker(EventChecker checker) {
    if (__loop.checkersCnt >= EVENTLOOP_MAX_CHECKERS) {
        return EV_LOOP_FULL;
    }
    __loop.checkers[__loop.checkersCnt++] = checker;
    return EV_LOOP_OK;
}

static EvLoopErr_t unregisterChecker(EventChecker checker) {
    EvLoopErr_t err = EV_LOOP_CHECKER_NOT_FOUND;
    for (size_t i = 0; i < __loop.checkersCnt; ++i) {
        if (__loop.checkers[i] == checker) {
            /* shift remaining callbacks left */
            for (size_t j = i + 1; j < __loop.checkersCnt; ++j) {
                __loop.checkers[j - 1] = __loop.checkers[j];
            }

            __loop.checkersCnt--;
            return EV_LOOP_OK;
        }
    }
    return err;
}

static EvLoopErr_t unregisterAll() {
    __loop.checkersCnt = 0;
    return EV_LOOP_OK;
}

OOP_CTOR(Eventloop) {
    self->runCycle = runCycle;
    self->registerChecker = registerChecker;
    self->unregisterChecker = unregisterChecker;
    self->unregisterAll = unregisterAll;
    self->checkersCnt = 0;
}

Eventloop *getEventloop(void) {
    CALL_ONCE(
        OOP_CALL_CTOR(Eventloop, &__loop);
    );
    return &__loop;
}