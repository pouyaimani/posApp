#include "states.h"
#include "event.h"
#include "network/network.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "sys/sys.h"

/*************************** Network connect ***********************/

STATE_DEF_ENTER(NetConnect) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CONNECTIING), "");
    NetConnect* st = (NetConnect*)(STATE_NET_CONNECT);
    network()->connect();
    if (network()->id < 0) {
        if (st->ctx.onFailureCb) {
            st->ctx.onFailureCb(st->ctx.userDataOnFailure);
        }
        GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                  phraseGetDef(PHRASE_CONNECTION_ERR), "");
        return;
    }
}

STATE_DEF_HANDLE(NetConnect, KeypadEvent) { SM_GOTO(STATE_IDLE); }

STATE_DEF_HANDLE(NetConnect, SocketConnectEvent) {
    NetConnect* st = (NetConnect*)(STATE_NET_CONNECT);
    if (!ev->isConnected) {
        if (st->ctx.onFailureCb) {
            st->ctx.onFailureCb(st->ctx.userDataOnFailure);
        }
        GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                  phraseGetDef(PHRASE_CONNECTION_ERR), "");
    } else {
        if (st->ctx.onSucessCb) {
            if (st->ctx.onSucessCb(st->ctx.userDataOnSucess) != ERR_OK) {
                GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                          phraseGetDef(PHRASE_PROCESS_CB_ERR), "");
                return;
            }
        }
        SM_GOTO(st->ctx.onSucess);
        HIDE_INFO();
    }
}

OOP_CTOR(NetConnect, State* parent, const char* name) {
    self->base.vtable.enter = STATE_ENTER(NetConnect);
    self->base.vtable.onSocketConnect =
        STATE_HANDLE(NetConnect, SocketConnectEvent);
}

/*************************** Network send ***********************/
STATE_DEF_ENTER(NetSend) {
    NetSend* st = (NetSend*)(STATE_NET_SEND);
    SHOW_INFO(INFO_ERROR, phraseGetDef(PHRASE_SENDING_DATA_ERR), "");
    int ret = network()->send(st->data.data, st->data.len);
    if (ret != st->data.len) {
        if (st->ctx.onFailureCb) {
            st->ctx.onFailureCb(st->ctx.userDataOnFailure);
        }
        GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                  phraseGetDef(PHRASE_SENDING_DATA_ERR), "");
        return;
    }
    if (st->ctx.onSucessCb) {
        if (st->ctx.onSucessCb(st->ctx.userDataOnSucess) != ERR_OK) {
            GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                      phraseGetDef(PHRASE_PROCESS_CB_ERR), "");
            return;
        }
    }
    // HIDE_INFO();
    SM_GOTO(st->ctx.onSucess);
}

STATE_DEF_HANDLE(NetSend, KeypadEvent) { SM_GOTO(STATE_IDLE); }

STATE_DEF_HANDLE(NetSend, SocketSentEvent) {}

OOP_CTOR(NetSend, State* parent, const char* name) {
    self->base.vtable.enter        = STATE_ENTER(NetSend);
    self->base.vtable.handleKeypad = STATE_HANDLE(NetSend, KeypadEvent);
    self->base.vtable.onSocketSent = STATE_HANDLE(NetSend, SocketSentEvent);
    self->data.data                = MEM_ALLOC(REC_BUFF_LEN);
    self->data.capacity            = REC_BUFF_LEN;
}

/*************************** Network receive ***********************/

STATE_DEF_ENTER(NetReceive) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_RECEIVING_DATA), "");
}

STATE_DEF_EXIT(NetReceive) { HIDE_INFO(); }

STATE_DEF_HANDLE(NetReceive, KeypadEvent) { SM_GOTO(STATE_IDLE); }

STATE_DEF_HANDLE(NetReceive, SocketReadyReadEvent) {
    NetReceive* st = (NetReceive*)(STATE_NET_RECEIVE);
    if (ev->ba.len <= 0) {
        if (st->ctx.onFailureCb) {
            st->ctx.onFailureCb(st->ctx.userDataOnFailure);
        }
        GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                  phraseGetDef(PHRASE_RECEIVING_DATA_ERR), "");
        return;
    }
    if (st->ctx.onSucessCb) {
        if (st->ctx.onSucessCb(st->ctx.userDataOnSucess) != ERR_OK) {
            GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                      phraseGetDef(PHRASE_RECEIVING_DATA_ERR),
                      phraseGetDef(PHRASE_PROCESS_CB_ERR));
            return;
        }
    }
    if (ev->ba.len > st->data.capacity) {
        GOTO_INFO(st->ctx.onFailure, st->ctx.onFailure, INFO_ERROR,
                  phraseGetDef(PHRASE_RECEIVING_DATA_ERR),
                  phraseGetDef(PHRASE_DATA_SIZE_MISMATCH));
        return;
    }
    memcpy(st->data.data, ev->ba.data, ev->ba.len);
    st->data.len = ev->ba.len;
    SM_GOTO(st->ctx.onSucess);
}

OOP_CTOR(NetReceive, State* parent, const char* name) {
    self->base.vtable.enter        = STATE_ENTER(NetReceive);
    self->base.vtable.exit         = STATE_EXIT(NetReceive);
    self->base.vtable.handleKeypad = STATE_HANDLE(NetReceive, KeypadEvent);
    self->base.vtable.onSocketReadyRead =
        STATE_HANDLE(NetReceive, SocketReadyReadEvent);
    self->data.data     = MEM_ALLOC(REC_BUFF_LEN);
    self->data.capacity = REC_BUFF_LEN;
}