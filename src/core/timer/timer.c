#include "timer.h"
#include "sys/sys.h"
#include "logger.h"
#include "error.h"

static TimerHandler* __timerHandler;

static TimerErr_t registerTimer(Timer* timer) {
    LOG_TRACE("Registring Timer ...");
    RETURN_VALUE_IF_NULL(timer, ;, TIMER_ERR_BAD_ARG);
    if (list_push_back(&__timerHandler->timers, timer) == false) {
        LOG_FATAL("Timer handler maximum is reached.");
        return TIMER_ERR_HANDLER_FULL;
    }
    LOG_TRACE("Timer is registered.");
    return TIMER_ERR_OK;
}

static TimerErr_t unRegisterTimer(Timer* timer) {
    LOG_TRACE("Unregistring Timer ...");

    if (!list_remove(&__timerHandler->timers, timer))
        return TIMER_ERR_NOT_FOUND;
    return TIMER_ERR_OK;
}

static void start(Timer* timer) {
    timer->isStoped = false;
    timer->ctime    = GET_TICK();
}

static void stop(Timer* timer) { timer->isStoped = true; }

OOP_CTOR(Timer, timerChecker checker, uint32_t period, bool singleShot) {
    self->checker      = checker;
    self->period       = period;
    self->isSingleShot = singleShot;
    self->isStoped     = true;
    self->start        = start;
    self->stop         = stop;
}

Timer* createTimer(timerChecker checker, uint32_t period, bool singleShot) {
    Timer* timer = (Timer*)MEM_ALLOC(sizeof(Timer));
    OOP_CALL_CTOR(Timer, timer, checker, period, singleShot);
    getTimerHanlder()->registerTimer(timer);
    return timer;
}

void removeTimer(Timer* timer) {
    getTimerHanlder()->unRegisterTimer(timer);
    MEM_FREE(timer);
}

static bool handleTimer(void* data, void* context) {
    VAR_UNUSED(context);
    Timer*   timer = (Timer*)data;
    uint32_t tick  = GET_TICK();
    if (tick > timer->ctime + timer->period) {
        if (timer->checker) {
            timer->checker(NULL);
        } else {
            LOG_ERROR("timer callback is not found.");
        }
        timer->ctime = tick;
    }
    return true;
}

static void runCycle() {
    list_foreach(&__timerHandler->timers, handleTimer, NULL);
}

OOP_CTOR(TimerHandler) {
    LOG_TRACE("Timer handler constructing ...");
    self->runCycle        = runCycle;
    self->registerTimer   = registerTimer;
    self->unRegisterTimer = unRegisterTimer;

    list_init(&self->timers, LIST_CIRCULAR);
}

TimerHandler* getTimerHanlder(void) {
    CALL_ONCE(__timerHandler = (TimerHandler*)MEM_ALLOC(sizeof(TimerHandler));
              OOP_CALL_CTOR(TimerHandler, __timerHandler););
    return __timerHandler;
}