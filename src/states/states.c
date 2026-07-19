#include "states.h"
#include "sys/sys.h"

static Startup*     startup;
static Idle*        idle;
static Input*       input;
static CardHolder*  cardHolder;
static Info*        info;
static Dialogue*    dialogue;
static Supporter*   supporter;
static DevInfo*     devInfo;
static StMenu*      menu;
static FixedAmount* fixedAmount;
static TxnResult*   txnRes;
static NetConnect*  netConnect;
static NetSend*     netSend;
static NetReceive*  netReceive;

State* getState(StateId_t id) {
    switch (id) {
    case STATE_ID_START_UP:
        CALL_ONCE(startup = MEM_ALLOC(sizeof(Startup));
                  OOP_CALL_CTOR(Startup, startup, NULL, "startup"););
        return (State*)startup;
    case STATE_ID_IDLE:
        CALL_ONCE(idle = MEM_ALLOC(sizeof(Idle));
                  OOP_CALL_CTOR(Idle, idle, startup, "idle"););
        return (State*)idle;
    // case STATE_ID_INPUT:
    //     CALL_ONCE(input = MEM_ALLOC(sizeof(Input));
    //               OOP_CALL_CTOR(Input, input, idle, "input"););
    //     return (State*)input;
    case STATE_ID_CARD_HOLDER:
        CALL_ONCE(cardHolder = MEM_ALLOC(sizeof(CardHolder));
                  OOP_CALL_CTOR(CardHolder, cardHolder, idle, "card holder"););
        return (State*)cardHolder;
    case STATE_ID_INFO:
        CALL_ONCE(info = MEM_ALLOC(sizeof(Info));
                  OOP_CALL_CTOR(Info, info, idle, "info"););
        return (State*)info;
    case STATE_ID_DIALOGUE:
        CALL_ONCE(dialogue = MEM_ALLOC(sizeof(Dialogue));
                  OOP_CALL_CTOR(Dialogue, dialogue, idle, "dialogue"););
        return (State*)dialogue;
    case STATE_ID_SUPPORTER:
        CALL_ONCE(supporter = MEM_ALLOC(sizeof(Supporter));
                  OOP_CALL_CTOR(Supporter, supporter, idle, "supporter"););
        return (State*)supporter;
    case STATE_ID_DEV_INFO:
        CALL_ONCE(devInfo = MEM_ALLOC(sizeof(DevInfo));
                  OOP_CALL_CTOR(DevInfo, devInfo, idle, "dev info"););
        return (State*)devInfo;
    case STATE_ID_MENU:
        CALL_ONCE(menu = MEM_ALLOC(sizeof(StMenu));
                  OOP_CALL_CTOR(StMenu, menu, idle, "menu"););
        return (State*)menu;
    case STATE_ID_FIXED_AMOUNT:
        CALL_ONCE(fixedAmount = MEM_ALLOC(sizeof(FixedAmount)); OOP_CALL_CTOR(
                      FixedAmount, fixedAmount, idle, "fixed Amount"););
        return (State*)fixedAmount;
    case STATE_ID_NET_CONNECT:
        CALL_ONCE(netConnect = MEM_ALLOC(sizeof(NetConnect));
                  OOP_CALL_CTOR(NetConnect, netConnect, idle, "net connect"););
        return (State*)netConnect;
    case STATE_ID_NET_SEND:
        CALL_ONCE(netSend = MEM_ALLOC(sizeof(NetSend));
                  OOP_CALL_CTOR(NetSend, netSend, idle, "net send"););
        return (State*)netSend;
    case STATE_ID_NET_RECEIVE:
        CALL_ONCE(netReceive = MEM_ALLOC(sizeof(NetReceive));
                  OOP_CALL_CTOR(NetReceive, netReceive, idle, "net receive"););
        return (State*)netReceive;
    case STATE_ID_TXN_RES:
        CALL_ONCE(txnRes = MEM_ALLOC(sizeof(TxnResult));
                  OOP_CALL_CTOR(TxnResult, txnRes, idle, "txn result"););
        return (State*)txnRes;
    default:
        break;
    }
}

// void GOTO_INPUT(State* prev, State* next, const char* title, const char*
// body,
//                 int max, InputMode_t mode, char* out) {
//     RETURN_IF_NULL(prev, ;);
//     RETURN_IF_NULL(next, ;);
//     Input* in = (Input*)getState(STATE_ID_INPUT);
//     in->reset();
//     in->out = out;
//     in->setData(title, body);
//     in->setMax(max);
//     in->setMode(mode);
//     OOP_CALL(getState(STATE_ID_INPUT), setPrev, prev);
//     OOP_CALL(getState(STATE_ID_INPUT), setNext, next);
//     SM_GOTO(getState(STATE_ID_INPUT));
// }

void GOTO_INFO(State* prev, State* next, uint8_t type, const char* title,
               const char* body) {
    RETURN_IF_NULL(prev, ;);
    RETURN_IF_NULL(next, ;);
    Info* info = (Info*)getState(STATE_ID_INFO);
    info->setData(type, title, body);
    OOP_CALL(getState(STATE_ID_INFO), setPrev, prev);
    OOP_CALL(getState(STATE_ID_INFO), setNext, next);
    SM_GOTO(getState(STATE_ID_INFO));
}

void GOTO_MENU(State* prev, Menu* amenu, CallBack_t _onExit, void* userData) {
    RETURN_IF_NULL(prev, ;);
    RETURN_IF_NULL(amenu, ;);
    StMenu* stMenu   = (StMenu*)getState(STATE_ID_MENU);
    stMenu->menu     = amenu;
    stMenu->onExit   = _onExit;
    stMenu->userData = userData;
    OOP_CALL(getState(STATE_ID_MENU), setPrev, prev);
    SM_GOTO(getState(STATE_ID_MENU));
}

void GOTO_TXN_RES(State* prev, State* next) {
    RETURN_IF_NULL(prev, ;);
    RETURN_IF_NULL(next, ;);
    OOP_CALL(getState(STATE_ID_TXN_RES), setNext, next);
    OOP_CALL(getState(STATE_ID_TXN_RES), setPrev, prev);
    TxnResult* res = getState(STATE_ID_TXN_RES);
    SM_GOTO(getState(STATE_ID_TXN_RES));
}

void GOTO_NET_CONNNECT(NetSubTaskCtx_t* ctx) {
    RETURN_IF_NULL(ctx, ;);
    netConnect->ctx.onFailure         = ctx->onFailure;
    netConnect->ctx.onFailureCb       = ctx->onFailureCb;
    netConnect->ctx.onSucess          = ctx->onSucess;
    netConnect->ctx.onSucessCb        = ctx->onSucessCb;
    netConnect->ctx.userDataOnFailure = ctx->userDataOnFailure;
    netConnect->ctx.userDataOnSucess  = ctx->userDataOnSucess;
    SM_GOTO(STATE_NET_CONNECT);
}

void GOTO_NET_SEND(NetSubTaskCtx_t* ctx, ByteArray* ba) {
    RETURN_IF_NULL(ba, ;);
    RETURN_IF_NULL(ctx, ;);
    STATE_NET_SEND;
    netSend->ctx.onFailure         = ctx->onFailure;
    netSend->ctx.onFailureCb       = ctx->onFailureCb;
    netSend->ctx.onSucess          = ctx->onSucess;
    netSend->ctx.onSucessCb        = ctx->onSucessCb;
    netSend->data.data             = ba->data;
    netSend->data.len              = ba->len;
    netSend->ctx.userDataOnFailure = ctx->userDataOnFailure;
    netSend->ctx.userDataOnSucess  = ctx->userDataOnSucess;
    SM_GOTO(STATE_NET_SEND);
}

void GOTO_NET_RECEIVE(NetSubTaskCtx_t* ctx, ByteArray* ba) {
    RETURN_IF_NULL(ba, ;);
    RETURN_IF_NULL(ctx, ;);
    netReceive->ctx.onFailure         = ctx->onFailure;
    netReceive->ctx.onFailureCb       = ctx->onFailureCb;
    netReceive->ctx.onSucess          = ctx->onSucess;
    netReceive->ctx.onSucessCb        = ctx->onSucessCb;
    netReceive->data.data             = ba->data;
    netReceive->data.len              = ba->len;
    netReceive->ctx.userDataOnFailure = ctx->userDataOnFailure;
    netReceive->ctx.userDataOnSucess  = ctx->userDataOnSucess;
    SM_GOTO(STATE_NET_RECEIVE);
}
