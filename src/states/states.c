#include "states.h"
#include "dev/dev.h"
#include "ui/ui.h"

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
static Communication *commu;
static TxnResult *txnRes;

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
    case STATE_ID_COMMU:
        CALL_ONCE(
            commu = GET_MEM(sizeof(Communication));
            OOP_CALL_CTOR(Communication, commu, idle, "communication");
        );
        return (State *)commu;
    case STATE_ID_TXN_RES:
        CALL_ONCE(
            txnRes = GET_MEM(sizeof(TxnResult));
            OOP_CALL_CTOR(TxnResult, txnRes, idle, "txn result");
        );
        return (State *)txnRes;
    default:
        break;
    }
}

void GOTO_INPUT(State *prev, State *next, const char *title,
        const char *body, int max, InputMode_t mode) {
    Input *in = (Input*)getState(STATE_ID_INPUT);
    in->reset();
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

void GOTO_MENU(State *prev, Menu * amenu, CallBack_t _onExit) {
    StMenu *stMenu = (StMenu *)getState(STATE_ID_MENU);
    stMenu->menu = amenu;
    stMenu->onExit = _onExit;
    OOP_CALL(getState(STATE_ID_MENU), setPrev, prev);
    SM_GOTO(getState(STATE_ID_MENU));
}

void GOTO_COMMU(State *prev, State *next) {
    OOP_CALL(getState(STATE_ID_COMMU), setNext, next);
    OOP_CALL(getState(STATE_ID_COMMU), setPrev, prev);
    SM_GOTO(getState(STATE_ID_COMMU));
}

void GOTO_TXN_RES(State *prev, State *next){
    OOP_CALL(getState(STATE_ID_TXN_RES), setNext, next);
    OOP_CALL(getState(STATE_ID_TXN_RES), setPrev, prev);
    SM_GOTO(getState(STATE_ID_TXN_RES));
}