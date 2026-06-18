#include "eventloop.h"
#include "logger.h"

Eventloop __loop;

static void runCycle() {
    for (size_t i = 0; i < __loop.checkersCnt; ++i) {
        __loop.checkers[i]();
    }
}

static EvLoopErr_t registerChecker(EventChecker checker) {
    LOG_TRACE("Event loop registring checker ...");
    if (__loop.checkersCnt >= EVENTLOOP_MAX_CHECKERS) {
        LOG_FATAL("Event loop maximum checker is reached.");
        return EV_LOOP_FULL;
    }
    __loop.checkers[__loop.checkersCnt++] = checker;
    LOG_TRACE("Event loop checker is registered.");
    return EV_LOOP_OK;
}

static EvLoopErr_t unregisterChecker(EventChecker checker) {
    LOG_TRACE("Event loop unregistring checker ...");
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
    LOG_TRACE("Event loop unregistring all checkers ...");
    __loop.checkersCnt = 0;
    return EV_LOOP_OK;
}

OOP_CTOR(Eventloop) {
    LOG_TRACE("Event loop constructing ...");
    self->runCycle          = runCycle;
    self->registerChecker   = registerChecker;
    self->unregisterChecker = unregisterChecker;
    self->unregisterAll     = unregisterAll;
    self->checkersCnt       = 0;
}

Eventloop* getEventloop(void) {
    CALL_ONCE(OOP_CALL_CTOR(Eventloop, &__loop););
    return &__loop;
}