#include "timer.h"
#include "dev/dev.h"
#include "logger.h"

static TimerHandler *__timerHandler;

static TimerErr_t registerTimer(Timer* timer) {
    LOG_TRACE("Registring Timer ...");
    if (__timerHandler->timersCnt >= TIMER_MAX_CHECKERS) {
        LOG_FATAL("Timer handler maximum is reached.");
        return TIMER_ERR_HANDLER_FULL;
    }
    __timerHandler->timers[__timerHandler->timersCnt++] = timer;
    LOG_TRACE("Timer is registered.");
    return TIMER_ERR_OK;
}

static TimerErr_t unRegisterTimer(Timer* timer) {
    LOG_TRACE("Unregistring Timer ...");
    TimerErr_t err = TIMER_ERR_NOT_FOUND;
    for (size_t i = 0; i < __timerHandler->timersCnt; ++i) {
        if (__timerHandler->timers[i] == timer) {
            /* shift remaining callbacks left */
            for (size_t j = i + 1; j <  __timerHandler->timersCnt; ++j) {
                __timerHandler->timers[j - 1] = __timerHandler->timers[j];
            }

            __timerHandler->timersCnt--;
            return TIMER_ERR_OK;
        }
    }
    return err;
}

// static EvLoopErr_t unregisterChecker(EventChecker checker) {
//     LOG_TRACE("Event loop unregistring checker ...");
//     EvLoopErr_t err = EV_LOOP_CHECKER_NOT_FOUND;
//     for (size_t i = 0; i < __loop.checkersCnt; ++i) {
//         if (__loop.checkers[i] == checker) {
//             /* shift remaining callbacks left */
//             for (size_t j = i + 1; j < __loop.checkersCnt; ++j) {
//                 __loop.checkers[j - 1] = __loop.checkers[j];
//             }

//             __loop.checkersCnt--;
//             return EV_LOOP_OK;
//         }
//     }
//     return err;
// }

// static EvLoopErr_t unregisterAll() {
//     LOG_TRACE("Event loop unregistring all checkers ...");
//     __loop.checkersCnt = 0;
//     return EV_LOOP_OK;
// }

static void start(Timer *timer) {
    timer->isStoped = false;
    timer->ctime = GET_TICK();
}

static void stop(Timer *timer) {
    timer->isStoped = true;
}

OOP_CTOR(Timer, timerChecker checker, uint32_t period, bool singleShot) {
    self->checker = checker;
    self->period = period;
    self->isSingleShot = singleShot;
    self->isStoped = true;
    self->start = start;
    self->stop = stop;
}

Timer *createTimer(timerChecker checker, uint32_t period, bool singleShot) {
    Timer *timer = (Timer*)GET_MEM(sizeof(Timer));
    OOP_CALL_CTOR(Timer, timer, checker, period, singleShot);
    getTimerHanlder()->registerTimer(timer);
    return timer;
}

void removeTimer(Timer *timer) {
    getTimerHanlder()->unRegisterTimer(timer);
    FREE_MEM(timer);
}

static void runCycle() {
    for (size_t i = 0; i < __timerHandler->timersCnt; ++i) {
        uint32_t tick = GET_TICK();
        if(tick - __timerHandler->timers[i]->ctime >= __timerHandler->timers[i]->period) {
            __timerHandler->timers[i]->checker();
            __timerHandler->timers[i]->ctime = tick;
        }
    }
}

OOP_CTOR(TimerHandler) {
    LOG_TRACE("Timer handler constructing ...");
    self->runCycle = runCycle;
    self->registerTimer = registerTimer;
    self->unRegisterTimer = unRegisterTimer;
    self->timersCnt = 0;
}

TimerHandler *getTimerHanlder(void) {
    CALL_ONCE(
        __timerHandler = (TimerHandler*)GET_MEM(sizeof(TimerHandler));
        OOP_CALL_CTOR(TimerHandler, __timerHandler);
    );
    return __timerHandler;
}