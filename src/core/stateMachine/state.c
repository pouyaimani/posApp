#include "state.h"
#include "core.h"
#include <stdio.h>
#include "logger.h"
#include "sys/sys.h"
#include "eventFactory.h"

#define DEFINE_DEFAULT_HANDLER(name, EventType, label)                         \
    static void name(State* state, EventType* event) {                         \
        (void)event;                                                           \
        defaultEventHandler(state, label);                                     \
    }

/* defaults */

static void default_enter(State* s) {
    // TODO -> develop log system
    printf("%s enter not defined\n", s->name);
}

static void default_exit(State* s) { printf("%s exit not defined\n", s->name); }

static void goTo(State* s, State* nextState) { smCore()->goTo(nextState); }

static void default_handleEvent(State* state, Event* event) {
    STM_LOG("Event %u is not handled by state %s", (unsigned)event->id,
            state->name);
}

static void setNext(State* current, State* next) { current->next = next; }

static void setPrev(State* current, State* prev) { current->prev = prev; }

static void setTimeOut(State* state, uint32_t timeout) {
    state->timeout = timeout;
    STM_LOG("State time out duration = %lu is set.", timeout);
}

static bool enableTimer(State* state) {
    if (!state) {
        return false;
    }
    if (state->isTimerEn) {
        return true;
    }
    state->timerList = MEM_ALLOC(sizeof(List));
    if (!state->timerList) {
        TRACE_POINT;
        return false;
    }
    state->isTimerEn = true;
    list_init(state->timerList, LIST_CIRCULAR);
    return true;
}

static bool disableTimer(State* state) {
    if (!state) {
        return false;
    }
    if (!state->isTimerEn && !state->timerList) {
        return true;
    }
    MEM_FREE(state->timerList);
    state->isTimerEn = false;
    return true;
}

static bool addTimer(State* state, StateCallback timerCb, uint32_t timeOut,
                     void* timerCbData) {
    if (!state) {
        return false;
    }
    if (!state->timerList) {
        return false;
    }
    StateTimer_t* timer = MEM_ALLOC(sizeof(*timer));
    if (!timer) {
        TRACE_POINT;
        return false;
    }
    timer->timerCb     = timerCb;
    timer->timeOut     = timeOut;
    timer->timerCbData = timerCbData;
    timer->ctime       = GET_TICK();
    return list_push_back(state->timerList, timer);
}

static bool match_timer_cb(void* data, void* context) {
    if (data == NULL || context == NULL)
        return false;
    TRACE_POINT;
    const StateTimer_t* timer = data;
    const StateCallback cb    = context;
    TRACE_POINT;

    return timer->timerCb == cb;
}

static bool removeTimer(State* state, StateCallback cb) {
    if (!state) {
        return false;
    }
    if (!state->timerList) {
        return false;
    }
    list_remove_predicate(state->timerList, match_timer_cb, cb);
}

typedef struct {
    StateCallback cb;
    uint32_t      timeout;
} TimerTimeoutChangeCtx;

static bool __resetTimerTimeOut(void* data, void* context) {
    TimerTimeoutChangeCtx* tcb   = (TimerTimeoutChangeCtx*)context;
    StateTimer_t*          timer = (StateTimer_t*)data;
    if (timer->timerCb == tcb->cb) {
        timer->timeOut = tcb->timeout;
        return false;
    }
    return true;
}

static bool resetTimerTimeOut(State* state, StateCallback cb,
                              uint32_t timeout) {
    if (!state) {
        return false;
    }
    if (!state->timerList) {
        return false;
    }
    TimerTimeoutChangeCtx tcb = {.cb = cb, .timeout = timeout};
    if (state->isTimerEn) {
        list_foreach(state->timerList, __resetTimerTimeOut, &tcb);
    }
}

static bool __updateTimers(void* data, void* context) {
    VAR_UNUSED(context);
    StateTimer_t* timer = (StateTimer_t*)data;
    timer->ctime        = GET_TICK();
    LOG_TRACE("timer updated...");
    return true;
}

static bool updateTimers(State* state) {
    LOG_TRACE("updating timers ...");
    if (!state) {
        return false;
    }
    if (!state->timerList) {
        return false;
    }
    if (state->isTimerEn) {
        list_foreach(state->timerList, __updateTimers, NULL);
    }
    LOG_TRACE("timers have been updated.");
    return true;
}

OOP_CTOR(State, State* parent, const char* name) {
    STM_LOG("Constructing State is started ...");
    self->vtable.enter             = default_enter;
    self->vtable.exit              = default_exit;
    self->vtable.handleTimeout     = default_handleEvent;
    self->vtable.handleKeypad      = default_handleEvent;
    self->vtable.handleMag         = default_handleEvent;
    self->vtable.handleWifi        = default_handleEvent;
    self->vtable.handleCell        = default_handleEvent;
    self->vtable.handlePed         = default_handleEvent;
    self->vtable.handlePicc        = default_handleEvent;
    self->vtable.goTo              = goTo;
    self->vtable.setNext           = setNext;
    self->vtable.setPrev           = setPrev;
    self->vtable.addTimer          = addTimer;
    self->vtable.removeTimer       = removeTimer;
    self->vtable.enableTimer       = enableTimer;
    self->vtable.resetTimerTimeOut = resetTimerTimeOut;
    self->vtable.updateTimers      = updateTimers;
    self->vtable.setTimeOut        = setTimeOut;
    self->parent                   = parent;
    self->next                     = NULL;
    self->prev                     = NULL;
    self->name                     = name;
    self->inner                    = STATE_ENTRY;
    self->timerList                = NULL;
    self->isTimerEn                = false;

    list_init(self->timerList, LIST_CIRCULAR);
    STM_LOG("Constructing State finished ...");
}

/* Generate the timeout factory. */
EVENT_DEFINE_FACTORY(TimeOutEvent);

static void to_dispatchTo(Event* self, State* state) {
    /*
     * Timeout handling will be discussed below.
     * This assumes State exposes handleTimeOut.
     */
    OOP_CALL(state, handleTimeout, self);
}

OOP_CTOR(TimeOutEvent) { self->base.vtable.dispatchTo = to_dispatchTo; }

void toEventRegister(void) { EVENT_REGISTER(SM_EVENT_TIME_OUT, TimeOutEvent); }