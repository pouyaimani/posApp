#include "nth_state.h"
#include "nth.h"
#include "phrases/phrases.h"
#include "settings/settings.h"
#include "event.h"
#include "txn.h"
#include "ui/infoPage.h"

/******************** Get Key sub state **********************/

STATE_DEF_HANDLE(NthState, SocketConnectEvent) {
    ByteArray ba;
    if (!ev->isConnected) {
        GOTO_INFO(((NthState*)state)->onFailure, 
            ((NthState*)state)->onFailure, phraseGetDef(PHRASE_CONNECTION_ERR), "");
        return;
    }
    SHOW_INFO(phraseGetDef(PHRASE_SENDING_DATA), "");
    if (nth()->send(((NthState*)state)->tx, ((NthState*)state)->txData) != NTH_OK) {
        GOTO_INFO(((NthState*)state)->onFailure, 
            ((NthState*)state)->onFailure, phraseGetDef(PHRASE_SENDING_DATA_ERR), "");
    }
}

STATE_DEF_HANDLE(NthState, SocketSentEvent) {
    SHOW_INFO(phraseGetDef(PHRASE_RECEIVING_DATA), "");
    LOG_DEBUG("socket sent event ...");
}

STATE_DEF_HANDLE(NthState, SocketReadyReadEvent) {
    LOG_DEBUG("socket rec event ...");
    GOTO_INFO(state->parent, 
            state->parent, phraseGetDef(PHRASE_SUC_DONME), "");
}

STATE_DEF_HANDLE(NthState, SocketTimeOutEvent) {
    Phrases_t title;
    Phrases_t body = PHRASE_TIME_OUT;
    switch (((NthState*)state)->tx->prevState) {
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
    GOTO_INFO(((NthState*)state)->onFailure, 
            ((NthState*)state)->onFailure, phraseGetDef(title),
                    phraseGetDef(body));
}

STATE_DEF_ENTER(NthState) {
    SHOW_INFO(phraseGetDef(PHRASE_CONNECTIING), "");
    DEFINE_STRING(ip, 24);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    NthResult res = nth()->connect(((NthState*)state)->tx, ip,
                settings()->server.mainServerPort);
    if (res == NTH_ERR_INVALID_HOST) {
        GOTO_INFO(((NthState*)state)->onFailure, 
            ((NthState*)state)->onFailure, phraseGetDef(PHRASE_INVALID_IP),
                 ip);
    } else if (res != NTH_OK) {
        GOTO_INFO(((NthState*)state)->onFailure, 
            ((NthState*)state)->onFailure, phraseGetDef(PHRASE_CONNECTION_ERR),
                    phraseGetDef(PHRASE_CHECK_NET));
    }
}

OOP_CTOR(NthState,
        NthTransaction *tx) {
    OOP_CALL_CTOR(State, self, tx->owner, "");
    self->tx = tx;
}