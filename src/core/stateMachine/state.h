#ifndef STATE_H
#define STATE_H

#include "oop.h"
#include <stdint.h>
#include "core.h"
#include "utility/linkedlist.h"

#define STATE_ENTER(type) type##_Enter

#define STATE_DEF_ENTER(type) static void type##_Enter(State* state)

#define STATE_EXIT(type) type##_Exit

#define STATE_DEF_EXIT(type) static void type##_Exit(State* state)

#define STATE_HANDLE(type, event) type##_handle_##event

#define STATE_DEF_HANDLE(type, event)                                          \
    static void type##_handle_##event(State* state, event##* ev)

#define SM_GOTO(state) smCore()->goTo(state)

#define SM_SUB(state) smCore()->goToSub(state)

/* Forward */
OOP_DECLARE_CLASS(Event);
OOP_DECLARE_CLASS(TimeOutEvent);
OOP_DECLARE_CLASS(KeypadEvent);
OOP_DECLARE_CLASS(MagEvent);
OOP_DECLARE_CLASS(WifiEvent);
OOP_DECLARE_CLASS(CellEvent);
OOP_DECLARE_CLASS(SocketConnectEvent);
OOP_DECLARE_CLASS(SocketReadyReadEvent);
OOP_DECLARE_CLASS(SocketSentEvent);
OOP_DECLARE_CLASS(SocketTimeOutEvent);
OOP_DECLARE_CLASS(State);

/* ===== Inner state ===== */

typedef enum StateInner {
    STATE_ENTRY = 0,
    STATE_EVENT,
    STATE_EXIT,
    STATE_SUBSTATE
} StateInner;

typedef void (*StateCallback)(void* arg);

typedef struct {
    uint32_t      lastCheckTime;
    uint32_t      trigDuration;
    StateCallback timerCb;
    void*         timerCbData;
} StateTimer_t;

/* ===== State vtable ===== */

OOP_VTABLE(State) {
    OOP_IMETHOD(void, State, enter);
    OOP_IMETHOD(void, State, exit);
    OOP_IMETHOD(void, State, handleTimeout, TimeOutEvent* ev);
    OOP_IMETHOD(void, State, handleKeypad, KeypadEvent* ev);
    OOP_IMETHOD(void, State, handleMag, MagEvent* ev);
    OOP_IMETHOD(void, State, handleWifi, WifiEvent* ev);
    OOP_IMETHOD(void, State, handleCell, CellEvent* ev);
    OOP_IMETHOD(void, State, handlePed, CellEvent* ev);
    OOP_IMETHOD(void, State, onSocketConnect, SocketConnectEvent* ev);
    OOP_IMETHOD(void, State, onSocketSent, SocketSentEvent* ev);
    OOP_IMETHOD(void, State, onSocketReadyRead, SocketReadyReadEvent* ev);
    OOP_IMETHOD(void, State, onSocketTimeOut, SocketTimeOutEvent* ev);
    // These methods shall not be overrided
    OOP_IMETHOD(void, State, goTo, State*);
    OOP_IMETHOD(void, State, setNext, State*);
    OOP_IMETHOD(void, State, setPrev, State*);
    OOP_IMETHOD(bool, State, enableTimer);
    OOP_IMETHOD(bool, State, disableTimer);
    OOP_IMETHOD(bool, State, addTimer, StateCallback timerCb,
                uint32_t trigDuration, void* timerCbData);
    OOP_IMETHOD(bool, State, removeTimer, StateCallback timerCb);
};

/* ===== State base ===== */

OOP_CLASS(State) {
    OOP_IMPLEMENTS(State);
    StateInner  inner;
    const char* name;
    State*      parent;
    State*      next;
    State*      prev;
    /* first etrance time */
    uint32_t entranceTime;
    List*    timerList;
    bool     isTimerEn;
};

/* ctor */
OOP_CTOR(State, State* parent, const char* name);

typedef State SubState;

#endif
