#include "state.h"
#include "core.h"
#include <stdio.h>
#include "logger.h"
#include "event.h"

/* defaults */

static void default_enter(State* s) {
    // TODO -> develop log system
    printf("%s enter not defined\n", s->name);
}

static void default_exit(State* s) { printf("%s exit not defined\n", s->name); }

static void goTo(State* s, State* nextState) { smCore()->goTo(nextState); }

static void default_timeout(State* s, TimeOutEvent* ev) {
    (void)ev;
    STM_LOG("Timeout event hadler is not defined for %s state.", s->name);
}

static void default_keypad(State* s, KeypadEvent* ev) {
    (void)ev;
    STM_LOG("Keypad event hadler is not defined for %s state.", s->name);
}

static void default_mag(State* s, MagEvent* ev) {
    (void)ev;
    STM_LOG("Mag event hadler is not defined for %s state.", s->name);
}

static void default_wifi(State* s, WifiEvent* ev) {
    (void)ev;
    STM_LOG("Wifi event hadler is not defined for %s state.", s->name);
}

static void default_cell(State* s, CellEvent* ev) {
    (void)ev;
    STM_LOG("Cellular event hadler is not defined for %s state.", s->name);
}

static void default_sock_connect(State* s, SocketConnectEvent* ev) {
    (void)ev;
    STM_LOG("Socket connect event hadler is not defined for %s state.",
            s->name);
}

static void default_sock_sent(State* s, SocketSentEvent* ev) {
    (void)ev;
    STM_LOG("Socket sent event hadler is not defined for %s state.", s->name);
}

static void default_sock_read(State* s, SocketReadyReadEvent* ev) {
    (void)ev;
    STM_LOG("Socket read event hadler is not defined for %s state.", s->name);
}

static void default_sock_timeout(State* s, SocketTimeOutEvent* ev) {
    (void)ev;
    STM_LOG("Socket timeout event hadler is not defined for %s state.",
            s->name);
}

static void setNext(State* current, State* next) { current->next = next; }

static void setPrev(State* current, State* prev) { current->prev = prev; }

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
    self->vtable.goTo              = goTo;
    self->vtable.setNext           = setNext;
    self->vtable.setPrev           = setPrev;
    self->parent                   = parent;
    self->next                     = NULL;
    self->prev                     = NULL;
    self->name                     = name;
    self->inner                    = STATE_ENTRY;
    STM_LOG("Constructing State finished ...");
}