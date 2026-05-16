#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "sys/sys.h"
#include "storage/storage.h"
#include "utility/utility.h"
#include "states/services/services.h"
#include "settings/settings.h"
#include "phrases/phrases.h"

static TerminalSettings *termStorage;

static const Phrases_t dsc[4] = {
    PHRASE_FIXED_AMNT,
    PHRASE_AMNT_CEIL,
    PHRASE_DIRECT_SALE,
    PHRASE_EN_SERVICES
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
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_FIRST_AMNT),
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 1: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_SEC_AMNT), 
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 2: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_THIRD_AMNT), 
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 3: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_FOURTH_AMNT),
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 4: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_FIFTH_AMNT), 
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 5: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_SIXTH_AMNT), 
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 6: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_SEVENTH_AMNT), 
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 7: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_EIGHTH_AMNT), 
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 8: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_NINTH_AMNT), 
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
            in->setInput(termStorage->amountList[listCnt]);
            break; }
        case 9: {
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_TENTH_AMNT),
                        phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
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
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_AMOUNT),
                         phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
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
            GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_AMOUNT),
                         phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT, IN_MODE_AMOUNT, NULL);
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
    OOP_CALL(&fixedAmntMenu, addItem, phraseGetDef(PHRASE_DISABLE), NULL, disableFixedAmnt, NULL);
    OOP_CALL(&fixedAmntMenu, addItem, phraseGetDef(PHRASE_PRICE_LIST), enterAmount, setFixedItemToList, NULL);
    OOP_CALL(&fixedAmntMenu, addItem, phraseGetDef(PHRASE_FIXED_AMNT), enterAmount, setFixedItemToSingle, NULL);
    OOP_CALL(&fixedAmntMenu, addItem, phraseGetDef(PHRASE_FIXED_WITH_COEF), enterAmount, setFixedItemToVariant, NULL);
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
    GOTO_INFO(state->parent, state->parent, phraseGetDef(PHRASE_ACTIVATE_SUC), "");
    termStorage->maxAmntEnable = true;
    snprintf(termStorage->maxAmnt,
        13, "%s", in->input);
}

STATE_DEF_ENTER(EnterMaxAmnt) {
    DEFINE_STRING(rial, 16);
    snprintf(rial, sizeof(rial), "(%s)", phraseGetDef(PHRASE_RIAL));
    GOTO_INPUT(state->parent, getMaxAmnt, phraseGetDef(PHRASE_AMNT_CEIL), rial, 12, IN_MODE_AMOUNT, NULL);
    Input *in = STATE_INPUT;
    in->setInput(termStorage->maxAmnt);
}

STATE_DEF_ENTER(MaxAmount) {
    uiMenu(&maxAmntMenu, getDisplay()->screen);
    maxAmntMenu.checkEnable = true;
    OOP_CALL(&maxAmntMenu, addItem, phraseGetDef(PHRASE_ENABLE), 
                enterMaxAmnt, NULL, NULL);
    OOP_CALL(&maxAmntMenu, addItem, phraseGetDef(PHRASE_DISABLE), 
                NULL, disMaxAmnt, NULL);
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
    settings()->terminal.maxAmntEnable = false;
}

STATE_DEF_ENTER(DirectSale) {
    uiMenu(&dirSaleMenu, getDisplay()->screen);
    dirSaleMenu.checkEnable = true;
    OOP_CALL(&dirSaleMenu, addItem, 
                phraseGetDef(PHRASE_ENABLE), NULL, enDirectSale, NULL);
    OOP_CALL(&dirSaleMenu, addItem, 
                phraseGetDef(PHRASE_DISABLE), NULL, disDirectSale, NULL);

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
    settings()->save();
}

STATE_DEF_ENTER(OtherProjects) {
    uiMenu(&otherMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < OTH_PROJ_ALL ; i++) {
        OOP_CALL(&otherMenu, addItem, phraseGetDef(dsc[i]), subState[i], NULL, NULL);
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
    termStorage = &settings()->terminal;
}