#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "storage/storage.h"
#include "utility/utility.h"
#include "states/services/services.h"

static TerminalSettings *termStorage;

static const char* otherItemTxt[4] = {
    "مبلغ ثابت",
    "سقف مبلغ",
    "خرید مستقیم",
    "فعالسازی سرویس ها"
};

typedef enum {
    OTH_PROJ_FIXED_AMONT = 0,
    OTH_PROJ_MAX_AMNT,
    OTH_PROJ_DIRECT_SALE,
    OTH_PROJ_EN_SERVICES,
    OTH_PROJ_ALL
} OtherProjectsItem_t;

static SubState *subState[OTH_PROJ_ALL];

/******************** fixed amount sub state **********************/

static const char* fixecAmntItemTxt[4] = {
    "غیر فعال",
    "لیست قیمت",
    "مبلغ ثابت",
    "ثابت با ضریب"
};

static Menu fixedAmntMenu;
static SubState *enterAmount;
static int listCnt = 0;

static disableFixedAmnt() {
    termStorage->fixedAmountItem = FIXED_AMNT_DIS;
}

static setFixedItemToList() {
    termStorage->fixedAmountItem = FIXED_AMNT_LISTS;
}

static setFixedItemToSingle() {
    termStorage->fixedAmountItem = FIXED_AMNT_SINGLE;
}

static setFixedItemToVariant() {
    termStorage->fixedAmountItem = FIXED_AMNT_VARIANT;
}

STATE_DEF_ENTER(EnterFixedAmount) {
    Input *in = STATE_INPUT;
    if (termStorage->fixedAmountItem == FIXED_AMNT_LISTS) {
        if (listCnt > 0) {
            Input *in = getState(STATE_ID_INPUT);
            snprintf(termStorage->amountList[listCnt - 1],
                13, "%s", in->input);
            termStorage->amountListCnt = listCnt;
        }
        switch (listCnt) {
        case 0: {
            GOTO_INPUT(state->parent, state, "مبلغ اول", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 1: {
            GOTO_INPUT(state->parent, state, "مبلغ دوم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 2: {
            GOTO_INPUT(state->parent, state, "مبلغ سوم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 3: {
            GOTO_INPUT(state->parent, state, "مبلغ چهارم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 4: {
            GOTO_INPUT(state->parent, state, "مبلغ پنجم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 5: {
            GOTO_INPUT(state->parent, state, "مبلغ ششم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 6: {
            GOTO_INPUT(state->parent, state, "مبلغ هفتم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 7: {
            GOTO_INPUT(state->parent, state, "مبلغ هشتم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 8: {
            GOTO_INPUT(state->parent, state, "مبلغ نهم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 9: {
            GOTO_INPUT(state->parent, state, "مبلغ دهم", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 10: {
            SM_GOTO(state->parent);
            break; }        
        default:
            break;
        }
        listCnt++;
    } else if (termStorage->fixedAmountItem == FIXED_AMNT_SINGLE) {
        if (listCnt == 0) {
            GOTO_INPUT(state->parent, state, "مبلغ", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[10]);
            listCnt++;
        } else {
            Input *in = getState(STATE_ID_INPUT);
            snprintf(termStorage->amountList[10],
                13, "%s", in->input);
            SM_GOTO(state->parent);
        }
    } else if (termStorage->fixedAmountItem == FIXED_AMNT_VARIANT) {
        if (listCnt == 0) {
            GOTO_INPUT(state->parent, state, "مبلغ", "ریال", AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[11]);
            listCnt++;
        } else {
            Input *in = getState(STATE_ID_INPUT);
            snprintf(termStorage->amountList[11],
                13, "%s", in->input);
            SM_GOTO(state->parent);
        }
    }
}

STATE_DEF_ENTER(FixedAmount) {
    uiMenu(&fixedAmntMenu, getDisplay()->screen);
    fixedAmntMenu.checkEnable = true;
    OOP_CALL(&fixedAmntMenu, addItem, fixecAmntItemTxt[FIXED_AMNT_DIS], NULL, disableFixedAmnt, NULL);
    OOP_CALL(&fixedAmntMenu, addItem, fixecAmntItemTxt[FIXED_AMNT_LISTS], enterAmount, setFixedItemToList, NULL);
    OOP_CALL(&fixedAmntMenu, addItem, fixecAmntItemTxt[FIXED_AMNT_SINGLE], enterAmount, setFixedItemToSingle, NULL);
    OOP_CALL(&fixedAmntMenu, addItem, fixecAmntItemTxt[FIXED_AMNT_VARIANT], enterAmount, setFixedItemToVariant, NULL);
    OOP_CALL(&fixedAmntMenu, setChecked, termStorage->fixedAmountItem);
    listCnt = 0;
    GOTO_MENU(state->parent, &fixedAmntMenu, NULL, NULL);
}

/******************** max amount sub state **********************/
static Menu maxAmntMenu;
static SubState *enterMaxAmnt;
static SubState *getMaxAmnt;

static void disMaxAmnt() {
    termStorage->maxAmntEnable = false;
}

STATE_DEF_ENTER(GetMaxAmnt) {
    Input *in = getState(STATE_ID_INPUT);
    GOTO_INFO(state->parent, state->parent, "با موفقیت فعال شد", "");
    termStorage->maxAmntEnable = true;
    snprintf(termStorage->maxAmnt,
        13, "%s", in->input);
}

STATE_DEF_ENTER(EnterMaxAmnt) {
    GOTO_INPUT(state->parent, getMaxAmnt, "سقف مبلغ", "(ریال)", 12, IN_MODE_AMOUNT, NULL);
    Input *in = STATE_INPUT;
    in->setInput(termStorage->maxAmnt);
}

STATE_DEF_ENTER(MaxAmount) {
    uiMenu(&maxAmntMenu, getDisplay()->screen);
    maxAmntMenu.checkEnable = true;
    OOP_CALL(&maxAmntMenu, addItem, "فعال", enterMaxAmnt, NULL, NULL);
    OOP_CALL(&maxAmntMenu, addItem, "غیر فعال", NULL, disMaxAmnt, NULL);
    if(termStorage->maxAmntEnable) {
        OOP_CALL(&maxAmntMenu, setChecked, 0);
    } else {
        OOP_CALL(&maxAmntMenu, setChecked, 1);
    }
    GOTO_MENU(state->parent, &maxAmntMenu, NULL, NULL);
}

/******************** direct sale sub state **********************/
static Menu dirSaleMenu;

static void enDirectSale() {
    termStorage->maxAmntEnable = false;
}

static void disDirectSale() {
    storage()->settings->terminal.maxAmntEnable = false;
}

STATE_DEF_ENTER(DirectSale) {
    uiMenu(&dirSaleMenu, getDisplay()->screen);
    dirSaleMenu.checkEnable = true;
    OOP_CALL(&dirSaleMenu, addItem, "فعال", NULL, enDirectSale, NULL);
    OOP_CALL(&dirSaleMenu, addItem, "غیر فعال", NULL, disDirectSale, NULL);

    GOTO_MENU(state->parent, &dirSaleMenu, NULL, NULL);
}

/******************** enable services sub state **********************/

static Menu servMenu;
static SubState *saveServiceStatus;

STATE_DEF_ENTER(SaveServiceStatus) {
    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        getService(i)->enable = servMenu.toggle[i];
        termStorage->serviceEn[i] = servMenu.toggle[i];
    }
    SM_GOTO(state->parent);
}

STATE_DEF_ENTER(EnableServices) {
    uiToggleMenu(&servMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        OOP_CALL(&servMenu, addOnOffItem, getService(i)->state.name,
            getService(i)->enable, NULL, NULL, NULL);
    }

    GOTO_MENU(saveServiceStatus, &servMenu, NULL, NULL);
}

/*********************** other project state **************************/

static Menu otherMenu;

static void saveSettings() {
    SAVE_SETTINGS();
}

STATE_DEF_ENTER(OtherProjects) {
    uiMenu(&otherMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < OTH_PROJ_ALL ; i++) {
        OOP_CALL(&otherMenu, addItem, otherItemTxt[i], subState[i], NULL, NULL);
    }
    GOTO_MENU(state->parent, &otherMenu, saveSettings, NULL);
}

OOP_CTOR(OtherProjects, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, "Other Projects");
    self->base.vtable.enter = STATE_ENTER(OtherProjects);

    subState[OTH_PROJ_FIXED_AMONT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_FIXED_AMONT], self, "fixed amount");
    subState[OTH_PROJ_FIXED_AMONT]->vtable.enter = STATE_ENTER(FixedAmount);

    subState[OTH_PROJ_MAX_AMNT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_MAX_AMNT], self, "max amount");
    subState[OTH_PROJ_MAX_AMNT]->vtable.enter = STATE_ENTER(MaxAmount);

    subState[OTH_PROJ_DIRECT_SALE] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_DIRECT_SALE], self, "direct sale");
    subState[OTH_PROJ_DIRECT_SALE]->vtable.enter = STATE_ENTER(DirectSale);

    subState[OTH_PROJ_EN_SERVICES] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_EN_SERVICES], self, "enable services");
    subState[OTH_PROJ_EN_SERVICES]->vtable.enter = STATE_ENTER(EnableServices);

    enterMaxAmnt = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterMaxAmnt, self, "enter max amount");
    enterMaxAmnt->vtable.enter = STATE_ENTER(EnterMaxAmnt);

    getMaxAmnt = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, getMaxAmnt, self, "get max amount");
    getMaxAmnt->vtable.enter = STATE_ENTER(GetMaxAmnt);

    saveServiceStatus = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, saveServiceStatus, self, "save service status");
    saveServiceStatus->vtable.enter = STATE_ENTER(SaveServiceStatus);

    enterAmount = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, subState[OTH_PROJ_FIXED_AMONT], "enter fixed amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterFixedAmount);
    termStorage = &storage()->settings->terminal;
}