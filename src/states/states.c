#include "states.h"
#include "sys/sys.h"
#include "ui/ui.h"
#include "transmitter.h"

static Startup *startup;
static Idle *idle;
static Input *input;
static CardHolder *cardHolder;
static Info *info;
static Ginfo *ginfo;
static Dialogue *dialogue;
static Supporter *supporter;
static DevInfo *devInfo;
static StMenu *menu;
static FixedAmount *fixedAmount;
static IsoTransmitter *isoTransmitter;
static TxnResult *txnRes;
static HttpTransmitter *httpTransmitter;
static NetConnect *netConnect;
static NetSend *netSend;
static NetReceive *netReceive;

State *getState(StateId_t id) {
    switch (id) {
    case STATE_ID_START_UP:
        CALL_ONCE(
            startup = GET_MEM(sizeof(Startup));
            OOP_CALL_CTOR(Startup, startup, NULL, "startup");
        );
        return (State *)startup;
    case STATE_ID_IDLE:
        CALL_ONCE(
            idle = GET_MEM(sizeof(Idle));
            OOP_CALL_CTOR(Idle, idle, startup, "idle");
        );
        return (State *)idle;
    case STATE_ID_INPUT:
        CALL_ONCE(
            input = GET_MEM(sizeof(Input));
            OOP_CALL_CTOR(Input, input, idle, "input");
        );
        return (State *)input;
    case STATE_ID_CARD_HOLDER:
        CALL_ONCE(
            cardHolder = GET_MEM(sizeof(CardHolder));
            OOP_CALL_CTOR(CardHolder, cardHolder, idle, "card holder");
        );
        return (State *)cardHolder;
    case STATE_ID_INFO:
        CALL_ONCE(
            info = GET_MEM(sizeof(Info));
            OOP_CALL_CTOR(Info, info, idle, "info");
        );
        return (State *)info;
    case STATE_ID_DIALOGUE:
        CALL_ONCE(
            dialogue = GET_MEM(sizeof(Dialogue));
            OOP_CALL_CTOR(Dialogue, dialogue, idle, "dialogue");
        );
        return (State *)dialogue;
    case STATE_ID_SUPPORTER:
        CALL_ONCE(
            supporter = GET_MEM(sizeof(Supporter));
            OOP_CALL_CTOR(Supporter, supporter, idle, "supporter");
        );
        return (State *)supporter;
    case STATE_ID_GINFO:
        CALL_ONCE(
            ginfo = GET_MEM(sizeof(Ginfo));
            OOP_CALL_CTOR(Ginfo, ginfo, idle, "ginfo");
        );
        return (State *)ginfo;
    case STATE_ID_DEV_INFO:
        CALL_ONCE(
            devInfo = GET_MEM(sizeof(DevInfo));
            OOP_CALL_CTOR(DevInfo, devInfo, idle, "dev info");
        );
        return (State *)devInfo;
    case STATE_ID_MENU:
        CALL_ONCE(
            menu = GET_MEM(sizeof(StMenu));
            OOP_CALL_CTOR(StMenu, menu, idle, "menu");
        );
        return (State *)menu;
    case STATE_ID_FIXED_AMOUNT:
        CALL_ONCE(
            fixedAmount = GET_MEM(sizeof(FixedAmount));
            OOP_CALL_CTOR(FixedAmount, fixedAmount, idle, "fixed Amount");
        );
        return (State *)fixedAmount;
    case STATE_ID_ISO_TRANSMITTER:
        CALL_ONCE(
            isoTransmitter = GET_MEM(sizeof(IsoTransmitter));
            OOP_CALL_CTOR(IsoTransmitter, isoTransmitter, idle, "iso transmitter");
        );
        return (State *)isoTransmitter;
    case STATE_ID_TXN_RES:
        CALL_ONCE(
            txnRes = GET_MEM(sizeof(TxnResult));
            OOP_CALL_CTOR(TxnResult, txnRes, idle, "txn result");
        );
        return (State *)txnRes;
    case STATE_ID_HTTP_TRANSMITTER:
        CALL_ONCE(
            httpTransmitter = GET_MEM(sizeof(HttpTransmitter));
            OOP_CALL_CTOR(HttpTransmitter, httpTransmitter, idle, "http transmitter");
        );
        return (State *)httpTransmitter;
    case STATE_ID_NET_CONNECT:
        CALL_ONCE(
            netConnect = GET_MEM(sizeof(NetConnect));
            OOP_CALL_CTOR(NetConnect, netConnect, idle, "net connect");
        );
        return (State *)netConnect;
    case STATE_ID_NET_SEND:
        CALL_ONCE(
            netSend = GET_MEM(sizeof(NetSend));
            OOP_CALL_CTOR(NetSend, netSend, idle, "net send");
        );
        return (State *)netSend;
    case STATE_ID_NET_RECEIVE:
        CALL_ONCE(
            netReceive = GET_MEM(sizeof(NetReceive));
            OOP_CALL_CTOR(NetReceive, netReceive, idle, "net receive");
        );
        return (State *)netReceive;
    default:
        break;
    }
}

void GOTO_INPUT(State *prev, State *next, const char *title,
        const char *body, int max, InputMode_t mode, char *out) {
    Input *in = (Input*)getState(STATE_ID_INPUT);
    in->reset();
    in->out = out;
    in->setData(title, body);
    in->setMax(max);
    in->setMode(mode);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, prev);
    OOP_CALL(getState(STATE_ID_INPUT), setNext, next);
    SM_GOTO(getState(STATE_ID_INPUT));
}

void GOTO_INFO(State *prev, State *next, const char *title, const char *body) {
    Info *info = (Info *)getState(STATE_ID_INFO);
    info->setText(title, body);
    OOP_CALL(getState(STATE_ID_INFO), setPrev, prev);
    OOP_CALL(getState(STATE_ID_INFO), setNext, next);
    SM_GOTO(getState(STATE_ID_INFO));
}

void GOTO_MENU(State *prev, Menu * amenu, CallBack_t _onExit, void *userData) {
    StMenu *stMenu = (StMenu *)getState(STATE_ID_MENU);
    stMenu->menu = amenu;
    stMenu->onExit = _onExit;
    stMenu->userData = userData;
    OOP_CALL(getState(STATE_ID_MENU), setPrev, prev);
    SM_GOTO(getState(STATE_ID_MENU));
}

void GOTO_ISO_TRANSMITTER(State *onFail, State *onSucess) {
    IsoTransmitter *isoTrns = STATE_TRNS_ISO;
    isoTrns->onSucess = onSucess;
    isoTrns->onFailure = onFail;
    SM_GOTO(STATE_TRNS_ISO);
}

void GOTO_HTTP_TRANSMITTER(State *onFail, State *onSucess) {
    HttpTransmitter *httpTrns = STATE_TRNS_HTTP;
    httpTrns->onSucess = onSucess;
    httpTrns->onFailure = onFail;
    SM_GOTO(STATE_TRNS_HTTP);
}

void GOTO_TXN_RES(State *prev, State *next){
    OOP_CALL(getState(STATE_ID_TXN_RES), setNext, next);
    OOP_CALL(getState(STATE_ID_TXN_RES), setPrev, prev);
    SM_GOTO(getState(STATE_ID_TXN_RES));
}

void GOTO_NET_CONNNECT(State *onFail, State *onSucess) {
    STATE_NET_CONNECT;
    netConnect->onSucess = onSucess;
    netConnect->onFailure = onFail;
    SM_GOTO(STATE_NET_CONNECT);
}

void GOTO_NET_SEND(const ByteArray *ba, State *onFail, State *onSucess) {
    STATE_NET_SEND;
    netSend->onSucess = onSucess;
    netSend->onFailure = onFail;
    netSend->ba = ba;
    SM_GOTO(STATE_NET_SEND);
}

void GOTO_NET_RECEIVE(ByteArray *ba,State *onFail, State *onSucess) {
    STATE_NET_RECEIVE;
    netReceive->onSucess = onSucess;
    netReceive->onFailure = onFail;
    netSend->ba = ba;
    SM_GOTO(STATE_NET_RECEIVE);
}

