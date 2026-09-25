#include "state.h"
#include "core.h"
#include <stdio.h>
#include "logger.h"
#include "event.h"
#include "sys/sys.h"

/* defaults */

static void default_enter(State* s) {
    // TODO -> develop log system
    printf("%s enter not defined\n", s->name);
}

static void default_exit(State* s) { printf("%s exit not defined\n", s->name); }

static void goTo(State* s, State* nextState) { smCore()->goTo(nextState); }

static void default_timeout(State* s, TimeOutEvent* ev) {
    (void)ev;
    STM_LOG("Timeout event handler is not defined for %s state.", s->name);
}

static void default_keypad(State* s, KeypadEvent* ev) {
    (void)ev;
    STM_LOG("Keypad event handler is not defined for %s state.", s->name);
}

static void default_mag(State* s, MagEvent* ev) {
    (void)ev;
    STM_LOG("Mag event handler is not defined for %s state.", s->name);
}

static void default_wifi(State* s, WifiEvent* ev) {
    (void)ev;
    STM_LOG("Wifi event handler is not defined for %s state.", s->name);
}

static void default_cell(State* s, CellEvent* ev) {
    (void)ev;
    STM_LOG("Cellular event handler is not defined for %s state.", s->name);
}

static void default_sock_connect(State* s, SocketConnectEvent* ev) {
    (void)ev;
    STM_LOG("Socket connect event handler is not defined for %s state.",
            s->name);
}

static void default_sock_sent(State* s, SocketSentEvent* ev) {
    (void)ev;
    STM_LOG("Socket sent event handler is not defined for %s state.", s->name);
}

static void default_sock_read(State* s, SocketReadyReadEvent* ev) {
    (void)ev;
    STM_LOG("Socket read event handler is not defined for %s state.", s->name);
}

static void default_sock_timeout(State* s, SocketTimeOutEvent* ev) {
    (void)ev;
    STM_LOG("Socket timeout event handler is not defined for %s state.",
            s->name);
}

static void default_ped(State* s, PedEvent* ev) {
    (void)ev;
    STM_LOG("Ped event handler is not defined for %s state.", s->name);
}

static void setNext(State* current, State* next) { current->next = next; }

static void setPrev(State* current, State* prev) { current->prev = prev; }

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

OOP_CTOR(State, State* parent, const char* name) {
    STM_LOG("Constructing State is started ...");
    self->vtable.enter             = default_enter;
    self->vtable.exit              = default_exit;
    self->vtable.handleTimeout     = default_timeout;
    self->vtable.handleKeypad      = default_keypad;
    self->vtable.handleMag         = default_mag;
    self->vtable.handleWifi        = default_wifi;
    self->vtable.handleCell        = default_cell;
    self->vtable.onSocketConnect   = default_sock_connect;
    self->vtable.onSocketSent      = default_sock_sent;
    self->vtable.onSocketReadyRead = default_sock_read;
    self->vtable.onSocketTimeOut   = default_sock_timeout;
    self->vtable.handlePed         = default_ped;
    self->vtable.goTo              = goTo;
    self->vtable.setNext           = setNext;
    self->vtable.setPrev           = setPrev;
    self->vtable.addTimer          = addTimer;
    self->vtable.removeTimer       = removeTimer;
    self->vtable.enableTimer       = enableTimer;
    self->vtable.resetTimerTimeOut = resetTimerTimeOut;
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