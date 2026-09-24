#ifndef TIMER_H_
#define TIMER_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "utility/linkedlist.h"

typedef enum TimerErr_t {
    TIMER_ERR_OK,
    TIMER_ERR_HANDLER_FULL,
    TIMER_ERR_NOT_FOUND,
    TIMER_ERR_BAD_ARG
} TimerErr_t;

typedef void (*timerChecker)();

OOP_CLASS(Timer) {
    bool     isSingleShot;
    bool     isStoped;
    uint32_t period;
    uint32_t ctime;
    OOP_METHOD(void, checker);
    OOP_METHOD(void, start, Timer*);
    OOP_METHOD(void, stop, Timer*);
};

OOP_CTOR(Timer, timerChecker, uint32_t, bool);

Timer* createTimer(timerChecker, uint32_t period, bool singleShot);
void   removeTimer(Timer* timer);

OOP_CLASS(TimerHandler) {
    List timers;
    OOP_METHOD(void, runCycle);
    OOP_METHOD(TimerErr_t, registerTimer, Timer*);
    OOP_METHOD(TimerErr_t, unRegisterTimer, Timer*);
};

OOP_CTOR(TimerHandler);

TimerHandler* getTimerHanlder(void);

#define TIMER_CREATE(checker, period, singleShot)                              \
    createTimer(checker, period, singleShot)
#define TIMER_START(timer)  timer->start(timer)
#define TIMER_STOP(timer)   timer->stop(timer)
#define TIMER_REMOVE(timer) removeTimer(timer)

#endif