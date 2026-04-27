#include "states.h"
#include "event.h"
#include "network/network.h"
#include "ui/ui.h"

/*************************** Network connect ***********************/

STATE_DEF_ENTER(NetConnect) {
    SHOW_INFO("در حال اتصال", "");
    network()->connect();
    if (network()->id < 0) {
        GOTO_INFO(((NetConnect*)(STATE_NET_CONNECT))->onFailure, 
            ((NetConnect*)(STATE_NET_CONNECT))->onFailure, "خطا در اتصال", "");
        return;
    }
}

STATE_DEF_HANDLE(NetConnect, KeypadEvent) {
    SM_GOTO(STATE_IDLE);
}

STATE_DEF_HANDLE(NetConnect, SocketConnectEvent) {
    if(ev->isConnected) {
        GOTO_INFO(((NetConnect*)state)->onFailure, 
            ((NetConnect*)state)->onFailure, "خطا در اتصال", "");
    } else {
        SM_GOTO(((NetConnect*)state)->onSucess);
        HIDE_INFO();
    }
}

OOP_CTOR(NetConnect, State *parent, const char *name) {
    self->base.vtable.enter = STATE_ENTER(NetConnect);
    self->base.vtable.onSocketConnect = STATE_HANDLE(NetConnect, SocketConnectEvent);
}

/*************************** Network send ***********************/
STATE_DEF_ENTER(NetSend) {
    NetSend* st = (NetSend*)(STATE_NET_SEND);
    SHOW_INFO("ارسال اطلاعات", "");
    int ret = network()->send(st->ba->data, st->ba->len);
    if (ret != st->ba->len) {
        GOTO_INFO(st->onFailure, 
            st->onFailure, "خطا در ارسال اطلاعات", "");
            return;
    }
    // HIDE_INFO();
    SM_GOTO(st->onSucess);
}

STATE_DEF_HANDLE(NetSend, KeypadEvent) {
    SM_GOTO(STATE_IDLE);
}

STATE_DEF_HANDLE(NetSend, SocketSentEvent) {
}

OOP_CTOR(NetSend, State *parent, const char *name) {
    self->base.vtable.enter = STATE_ENTER(NetSend);
    self->base.vtable.handleKeypad = STATE_HANDLE(NetSend, KeypadEvent);
    self->base.vtable.onSocketSent = STATE_HANDLE(NetSend, SocketSentEvent);

}

/*************************** Network receive ***********************/

STATE_DEF_ENTER(NetReceive) {
    SHOW_INFO("دریافت اطلاعات", "");
}

STATE_DEF_EXIT(NetReceive) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(NetReceive, KeypadEvent) {
    SM_GOTO(STATE_IDLE);
}

STATE_DEF_HANDLE(NetReceive, SocketReadyReadEvent) {
    NetReceive* st = (NetSend*)(STATE_NET_RECEIVE);
    if (ev->ba.len > 0) {
        GOTO_INFO(st->onFailure, 
            st->onFailure, "خطا در دریافت اطلاعات", "");
            return;
    }
    SM_GOTO(st->onSucess);
}

OOP_CTOR(NetReceive, State *parent, const char *name) {
    self->base.vtable.enter = STATE_ENTER(NetReceive);
    self->base.vtable.exit = STATE_EXIT(NetReceive);
    self->base.vtable.handleKeypad = STATE_HANDLE(NetReceive, KeypadEvent);
    self->base.vtable.onSocketSent = STATE_HANDLE(NetReceive, SocketReadyReadEvent);
}