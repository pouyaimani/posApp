#include "nth_state.h"
#include "nth.h"
#include "phrases/phrases.h"
#include "settings/settings.h"
#include "event.h"
#include "txn.h"
#include "ui/infoPage.h"

static NthState *self(State *s) {
    return (NthState*)s;
}

/******************** Get Key sub state **********************/

STATE_DEF_HANDLE(NthState, SocketConnectEvent) {
    ByteArray ba;
    if (!ev->isConnected) {
        GOTO_INFO(self(state)->failure, 
            self(state)->failure, INFO_ERROR, phraseGetDef(PHRASE_CONNECTION_ERR), "");
        return;
    }
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_SENDING_DATA), "");
    if (nth()->sendProvidedTx(self(state)->tx) != NTH_OK) {
        GOTO_INFO(self(state)->failure, 
            self(state)->failure, INFO_ERROR, phraseGetDef(PHRASE_SENDING_DATA_ERR), "");
    }
}

STATE_DEF_HANDLE(NthState, SocketSentEvent) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_RECEIVING_DATA), "");
    LOG_DEBUG("socket sent event ...");
}

STATE_DEF_HANDLE(NthState, SocketTimeOutEvent) {
    Phrases_t title;
    Phrases_t body = PHRASE_TIME_OUT;
    switch (self(state)->tx->prevState) {
    case NTH_TX_CONNECTING:
        title = PHRASE_CONNECTION_ERR;
        break;
    case NTH_TX_SENDING:
        title = PHRASE_SENDING_DATA_ERR;
        break;
    case NTH_TX_RECEIVING:
        title = PHRASE_RECEIVING_DATA_ERR;
        break;
    case NTH_TX_FAILED:
        return;
    default:
        break;
    }
    GOTO_INFO(self(state)->failure, 
            self(state)->failure, INFO_ERROR, phraseGetDef(title),
                    phraseGetDef(body));
}

STATE_DEF_ENTER(NthState) {
    self(state)->tx->owner = state;
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CONNECTIING), "");
    DEFINE_STRING(ip, 24);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    NthResult res = nth()->connect(self(state)->tx, ip,
                settings()->server.mainServerPort);
    if (res == NTH_ERR_INVALID_HOST) {
        GOTO_INFO(self(state)->failure, 
            self(state)->failure, INFO_ERROR, phraseGetDef(PHRASE_INVALID_IP),
                 ip);
    } else if (res != NTH_OK) {
        GOTO_INFO(self(state)->failure, 
            self(state)->failure, INFO_ERROR, phraseGetDef(PHRASE_CONNECTION_ERR),
                    phraseGetDef(PHRASE_CHECK_NET));
    }
}

OOP_CTOR(NthState,
        NthTransaction *tx) {
    OOP_CALL_CTOR(State, self, NULL, "");
    self->tx = tx;
    self->base.vtable.enter = STATE_ENTER(NthState);
    self->base.vtable.onSocketConnect = STATE_HANDLE(NthState, SocketConnectEvent);
    self->base.vtable.onSocketSent = STATE_HANDLE(NthState, SocketSentEvent);
    self->base.vtable.onSocketTimeOut = STATE_HANDLE(NthState, SocketTimeOutEvent);
}

void GOTO_NTH(NthState *state) {
    RETURN_IF_NULL(state, ;);
    SM_GOTO((State*)state);
}