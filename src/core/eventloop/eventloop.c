#include "eventloop.h"

static Eventloop loop;

static void runCycle() {
    for (size_t i = 0; i < loop.checkersCnt; ++i) {
        loop.checkers[i]();
    }
}

static EvLoopErr_t registerChecker(EventChecker checker) {
    if (loop.checkersCnt >= EVENTLOOP_MAX_CHECKERS) {
        return EV_LOOP_FULL;
    }
    loop.checkers[loop.checkersCnt++] = checker;
    return EV_LOOP_OK;
}

static EvLoopErr_t unregisterChecker(EventChecker checker) {
    EvLoopErr_t err = EV_LOOP_CHECKER_NOT_FOUND;
    for (size_t i = 0; i < loop.checkersCnt; ++i) {
        if (loop.checkers[i] == checker) {
            /* shift remaining callbacks left */
            for (size_t j = i + 1; j < loop.checkersCnt; ++j) {
                loop.checkers[j - 1] = loop.checkers[j];
            }

            loop.checkersCnt--;
            return EV_LOOP_OK;
        }
    }
    return err;
}

static EvLoopErr_t unregisterAll() {
    loop.checkersCnt = 0;
    return EV_LOOP_OK;
}

void Eventloop_ctor(Eventloop* self) {
    self->runCycle = runCycle;
    self->registerChecker = registerChecker;
    self->unregisterChecker = unregisterChecker;
    self->unregisterAll = unregisterAll;
    self->checkersCnt = 0;
}

Eventloop *getEventloop(void) {
    CALL_ONCE(
        Eventloop_ctor(&loop);
    );
    return &loop;
}