#include "supervisor.h"
#include "common.h"
#include "states/states.h"
#include "phrases/phrases.h"
#include "display/display.h"
#include "sys/sys.h"
#include "iso8583.h"
#include "utility/utility.h"
#include "network/network.h"
#include "isoBuilder.h"
#include "transmitter.h"
#include "nth/nth.h"
#include "settings/settings.h"
#include "ui/menu.h"
#include "ui/infoPage.h"

OOP_CLASS(LogOn) {
    OOP_EXTENDS(State);
};

OOP_CLASS(GetMerchantData) {
    OOP_EXTENDS(State);
};

static LogOn *logOn;
static GetMerchantData *getMerchantData;
static NthTransaction *tx;

/******************** Get Key sub state **********************/

STATE_DEF_HANDLE(LogOn, SocketConnectEvent) {
    ByteArray ba;
    if (!ev->isConnected) {
        nth()->release(tx);
        GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(PHRASE_CONNECTION_ERR), "");
        return;
    }
    byteArrayInit(&ba, NT_TX_BUFFER_SIZE);
    if (isoBuild(MTI_LOG_ON, &ba) != ERR_OK) {
        nth()->release(tx);
        GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(PHRASE_SENDING_DATA_ERR), "");
        LOG_DEBUG("Building iso failed...");
        return;
    }
    LOG_DEBUG("Building iso succeed...");
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_SENDING_DATA), "");
    if (nth()->send(tx, &ba) != NTH_OK) {
        nth()->release(tx);
        GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, 
                phraseGetDef(PHRASE_SENDING_DATA_ERR), "");
    }
    byteArrayDestroy(&ba);
}

STATE_DEF_HANDLE(LogOn, SocketSentEvent) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_RECEIVING_DATA), "");
    LOG_DEBUG("socket sent event ...");
}

STATE_DEF_HANDLE(LogOn, SocketReadyReadEvent) {
    LOG_DEBUG("socket rec event ...");
    RespCode_t resp = isoParse(MTI_LOG_ON, &ev->ba);
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    if(resp == ERR_NOK) {
        GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(PHRASE_PARSE_ERROR), "");
    } else if (resp == 0){
        GOTO_INFO(state->parent, 
            state->parent, INFO_SUCCESS, phraseGetDef(PHRASE_SUC_DONME), "");
    } else {
        DEFINE_STRING(dsc, 128);
        getResponseCode(resp, dsc, sizeof(dsc));
        GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION), dsc);
    }
    nth()->release(tx);
}

STATE_DEF_HANDLE(LogOn, SocketTimeOutEvent) {
    Phrases_t title;
    Phrases_t body = PHRASE_TIME_OUT;
    switch (tx->prevState) {
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
    GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(title),
                    phraseGetDef(body));
    nth()->release(tx);
}

STATE_DEF_ENTER(LogOn) {
    tx = nth()->alloc();
    RETURN_IF_NULL(tx, 
            GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(PHRASE_CONNECTION_ERR), "");
        );
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CONNECTIING), "");
    tx->owner = state;
    DEFINE_STRING(ip, 24);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    NthResult res = nth()->connect(tx, ip,
                settings()->server.mainServerPort);
    if (res == NTH_ERR_INVALID_HOST) {
        GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(PHRASE_INVALID_IP),
                 ip);
        nth()->release(tx);
    } else if (res != NTH_OK) {
        GOTO_INFO(state->parent, 
            state->parent, INFO_ERROR, phraseGetDef(PHRASE_CONNECTION_ERR),
                    phraseGetDef(PHRASE_CHECK_NET));
        nth()->release(tx);
    }
}

// void preaperData(NthTransaction *tx, void *userData) {
//     ByteArray ba;
//     byteArrayInit(&ba, NT_TX_BUFFER_SIZE);
//     if (isoBuild(MTI_LOG_ON, &ba) != ERR_OK) {
//         nth()->release(tx);
//         GOTO_INFO(tx->process.failure, 
//             tx->process.failure, INFO_ERROR, phraseGetDef(PHRASE_SENDING_DATA_ERR), "");
//         LOG_DEBUG("Building iso failed...");
//         return;
//     }
//     nth()->setTx(tx, &ba);
//     byteArrayDestroy(&ba);
// }

// void onRec(NthTransaction *tx, void *userData) {
//     RespCode_t resp = isoParse(MTI_LOG_ON, &tx->rxBuffer);
//     if(resp == ERR_NOK) {
//         GOTO_INFO(tx->process.failure, 
//             tx->process.failure, INFO_ERROR, phraseGetDef(PHRASE_PARSE_ERROR), "");
//     } else if (resp == 0){
//         GOTO_INFO(tx->process.success, 
//             tx->process.success, INFO_SUCCESS, phraseGetDef(PHRASE_SUC_DONME), "");
//     } else {
//         DEFINE_STRING(dsc, 128);
//         getResponseCode(resp, dsc, sizeof(dsc));
//         GOTO_INFO(tx->process.failure, 
//             tx->process.failure, INFO_ERROR, phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION), dsc);
//     }
//     nth()->release(tx);
// }

// void onfailure(NthTransaction *tx, void *userData) {

//     nth()->release(tx);
// }

// STATE_DEF_ENTER(LogOn) {
//     tx = nth()->alloc();
//     RETURN_IF_NULL(tx, 
//             GOTO_INFO(state->parent, 
//             state->parent, INFO_ERROR, phraseGetDef(PHRASE_CONNECTION_ERR), "");
//     );
//     tx->onConnect = preaperData;
//     tx->onReceive = onRec;
//     tx->onFailure = preaperData;
//     tx->onTimeout = preaperData;
//     tx->process.failure = state->parent;
//     tx->process.success = state->parent;
//     GOTO_NTH(&tx->process);
// }

/******************** Get merchant data sub state **********************/

STATE_DEF_ENTER(GetMerchantData) {

}

/******************** Configuration sub state **********************/

static Menu *cfgMenu;

STATE_DEF_ENTER(Configuration) {
    ui_menu_create(cfgMenu, disp()->screen);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_KEY), logOn, NULL, NULL);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_MERCHANT_DATA), getMerchantData, NULL, NULL);
    GOTO_MENU(state->parent, cfgMenu, NULL, NULL);
}

OOP_CTOR(Configuration, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Configuration);

    logOn = (LogOn *)MEM_ALLOC(sizeof(LogOn));
    OOP_CALL_CTOR(State, &logOn->base, self, "");
    logOn->base.vtable.enter = STATE_ENTER(LogOn);
    logOn->base.vtable.onSocketConnect = STATE_HANDLE(LogOn, SocketConnectEvent);
    logOn->base.vtable.onSocketSent = STATE_HANDLE(LogOn, SocketSentEvent);
    logOn->base.vtable.onSocketReadyRead = STATE_HANDLE(LogOn, SocketReadyReadEvent);
    logOn->base.vtable.onSocketTimeOut = STATE_HANDLE(LogOn, SocketTimeOutEvent);

    getMerchantData = (GetMerchantData *)MEM_ALLOC(sizeof(GetMerchantData));
    OOP_CALL_CTOR(State, &getMerchantData->base, self, "");
    getMerchantData->base.vtable.enter = STATE_ENTER(GetMerchantData);

    cfgMenu = (Menu *)MEM_ALLOC(sizeof(Menu));
}