#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/menu.h"
#include "sys/sys.h"
#include "storage/storage.h"
#include "utility/utility.h"
#include "services/services.h"
#include "settings/settings.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "input/inputMgr.h"

static TerminalSettings* termStorage;

static const Phrases_t dsc[4] = {PHRASE_FIXED_AMNT, PHRASE_AMNT_CEIL,
                                 PHRASE_DIRECT_SALE, PHRASE_EN_SERVICES};

typedef enum {
    OTH_PROJ_FIXED_AMONT = 0,
    OTH_PROJ_MAX_AMNT,
    OTH_PROJ_DIRECT_SALE,
    OTH_PROJ_EN_SERVICES,
    OTH_PROJ_ALL
} OtherProjectsItem_t;

static SubState* subState[OTH_PROJ_ALL];

/******************** fixed amount sub state **********************/

static Menu*     fixedAmntMenu;
static SubState* enterAmount;
static int       listCnt = 0;

static disableFixedAmnt() { termStorage->fixedAmountItem = FIXED_AMNT_DIS; }

static setFixedItemToList() { termStorage->fixedAmountItem = FIXED_AMNT_LISTS; }

static setFixedItemToSingle() {
    termStorage->fixedAmountItem = FIXED_AMNT_SINGLE;
}

static setFixedItemToVariant() {
    termStorage->fixedAmountItem = FIXED_AMNT_VARIANT;
}

STATE_DEF_ENTER(EnterFixedAmount) {
    if (termStorage->fixedAmountItem == FIXED_AMNT_LISTS) {
        if (listCnt > 0) {
            snprintf(termStorage->amountList[listCnt - 1], 13, "%s",
                     inmgr()->input);
            termStorage->amountListCnt = listCnt;
        }
        Phrases_t phrase;
        switch (listCnt) {
        case 0: {
            phrase = PHRASE_FIRST_AMNT;
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_FIRST_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 1: {
            phrase = PHRASE_SEC_AMNT;
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_SEC_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 2: {
            phrase = PHRASE_THIRD_AMNT;
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_THIRD_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 3: {
            phrase = PHRASE_FOURTH_AMNT;
            // GOTO_INPUT(state->parent, state,
            // phraseGetDef(PHRASE_FOURTH_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 4: {
            phrase = PHRASE_FIFTH_AMNT;
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_FIFTH_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 5: {
            phrase = PHRASE_SIXTH_AMNT;
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_SIXTH_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 6: {
            phrase = PHRASE_SEVENTH_AMNT;
            // GOTO_INPUT(state->parent, state,
            // phraseGetDef(PHRASE_SEVENTH_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 7: {
            phrase = PHRASE_EIGHTH_AMNT;
            // GOTO_INPUT(state->parent, state,
            // phraseGetDef(PHRASE_EIGHTH_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 8: {
            phrase = PHRASE_NINTH_AMNT;
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_NINTH_AMNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 9: {
            phrase = PHRASE_TENTH_AMNT;
            // GOTO_INPUT(state->parent, state, phraseGetDef(phrase),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[listCnt]);
            break;
        }
        case 10: {
            SM_GOTO(state->parent);
            return;
        }
        default:
            return;
        }
        inmgr()->run(
            &(InputCfg){
                .type   = INPUT_TYPE_KEYPAD,
                .mode   = INMD_ENTER_AMOUNT,
                .title  = phraseGetDef(phrase),
                .info   = phraseGetDef(PHRASE_RIAL),
                .maxLen = AMOUNT_MAX_CNT,
            },
            state->parent, state);
        inmgr()->set(INPUT_TYPE_KEYPAD, termStorage->amountList[listCnt]);
        // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_TENTH_AMNT),
        //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
        //            IN_MODE_AMOUNT, NULL);
        // in->setInput(termStorage->amountList[listCnt]);
        listCnt++;
    } else if (termStorage->fixedAmountItem == FIXED_AMNT_SINGLE) {
        if (listCnt == 0) {
            inmgr()->run(
                &(InputCfg){
                    .type   = INPUT_TYPE_KEYPAD,
                    .mode   = INMD_ENTER_AMOUNT,
                    .title  = phraseGetDef(PHRASE_AMOUNT),
                    .info   = phraseGetDef(PHRASE_RIAL),
                    .maxLen = AMOUNT_MAX_CNT,
                },
                state->parent, state);
            inmgr()->set(INPUT_TYPE_KEYPAD, termStorage->amountList[10]);
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_AMOUNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[10]);
            listCnt++;
        } else {
            snprintf(termStorage->amountList[10], 13, "%s", inmgr()->input);
            SM_GOTO(state->parent);
        }
    } else if (termStorage->fixedAmountItem == FIXED_AMNT_VARIANT) {
        if (listCnt == 0) {
            inmgr()->run(
                &(InputCfg){
                    .type   = INPUT_TYPE_KEYPAD,
                    .mode   = INMD_ENTER_AMOUNT,
                    .title  = phraseGetDef(PHRASE_AMOUNT),
                    .info   = "",
                    .maxLen = AMOUNT_MAX_CNT,
                },
                state->parent, state);
            inmgr()->set(INPUT_TYPE_KEYPAD, termStorage->amountList[11]);
            // GOTO_INPUT(state->parent, state, phraseGetDef(PHRASE_AMOUNT),
            //            phraseGetDef(PHRASE_RIAL), AMOUNT_MAX_CNT,
            //            IN_MODE_AMOUNT, NULL);
            // in->setInput(termStorage->amountList[11]);
            listCnt++;
        } else {
            snprintf(termStorage->amountList[11], 13, "%s", inmgr()->input);
            SM_GOTO(state->parent);
        }
    }
}

STATE_DEF_ENTER(FixedAmount) {
    ui_menu_create(fixedAmntMenu, disp()->screen);
    fixedAmntMenu->checkEnable = true;
    ui_menu_addItem(fixedAmntMenu, phraseGetDef(PHRASE_DISABLE), NULL,
                    disableFixedAmnt, NULL);
    ui_menu_addItem(fixedAmntMenu, phraseGetDef(PHRASE_PRICE_LIST), enterAmount,
                    setFixedItemToList, NULL);
    ui_menu_addItem(fixedAmntMenu, phraseGetDef(PHRASE_FIXED_AMNT), enterAmount,
                    setFixedItemToSingle, NULL);
    ui_menu_addItem(fixedAmntMenu, phraseGetDef(PHRASE_FIXED_WITH_COEF),
                    enterAmount, setFixedItemToVariant, NULL);
    ui_menu_set_checked(fixedAmntMenu, termStorage->fixedAmountItem);
    listCnt = 0;
    GOTO_MENU(state->parent, fixedAmntMenu, NULL, NULL);
}

/******************** max amount sub state **********************/
static Menu*     maxAmntMenu;
static SubState* enterMaxAmnt;
static SubState* getMaxAmnt;

static void disMaxAmnt() { termStorage->maxAmntEnable = false; }

STATE_DEF_ENTER(GetMaxAmnt) {
    GOTO_INFO(state->parent, state->parent, INFO_SUCCESS,
              phraseGetDef(PHRASE_ACTIVATE_SUC), "");
    termStorage->maxAmntEnable = true;
    snprintf(termStorage->maxAmnt, 13, "%s", inmgr()->input);
}

STATE_DEF_ENTER(EnterMaxAmnt) {
    DEFINE_STRING(rial, 16);
    snprintf(rial, sizeof(rial), "(%s)", phraseGetDef(PHRASE_RIAL));
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_AMOUNT,
            .title  = phraseGetDef(PHRASE_AMNT_CEIL),
            .info   = rial,
            .maxLen = AMOUNT_MAX_CNT,
        },
        state->parent, getMaxAmnt);
    // GOTO_INPUT(state->parent, getMaxAmnt, phraseGetDef(PHRASE_AMNT_CEIL),
    // rial,
    //            12, IN_MODE_AMOUNT, NULL);
    inmgr()->set(INPUT_TYPE_KEYPAD, termStorage->maxAmnt);
}

STATE_DEF_ENTER(MaxAmount) {
    ui_menu_create(maxAmntMenu, disp()->screen);
    maxAmntMenu->checkEnable = true;
    ui_menu_addItem(maxAmntMenu, phraseGetDef(PHRASE_ENABLE), enterMaxAmnt,
                    NULL, NULL);
    ui_menu_addItem(maxAmntMenu, phraseGetDef(PHRASE_DISABLE), NULL, disMaxAmnt,
                    NULL);
    if (termStorage->maxAmntEnable) {
        ui_menu_set_checked(maxAmntMenu, 0);
    } else {
        ui_menu_set_checked(maxAmntMenu, 1);
    }
    GOTO_MENU(state->parent, maxAmntMenu, NULL, NULL);
}

/******************** direct sale sub state **********************/
static Menu* dirSaleMenu;

static void enDirectSale() { termStorage->maxAmntEnable = false; }

static void disDirectSale() { settings()->terminal.maxAmntEnable = false; }

STATE_DEF_ENTER(DirectSale) {
    ui_menu_create(dirSaleMenu, disp()->screen);
    dirSaleMenu->checkEnable = true;
    ui_menu_addItem(dirSaleMenu, phraseGetDef(PHRASE_ENABLE), NULL,
                    enDirectSale, NULL);
    ui_menu_addItem(dirSaleMenu, phraseGetDef(PHRASE_DISABLE), NULL,
                    disDirectSale, NULL);

    GOTO_MENU(state->parent, dirSaleMenu, NULL, NULL);
}

/******************** enable services sub state **********************/

static Menu*     servMenu;
static SubState* saveServiceStatus;

STATE_DEF_ENTER(SaveServiceStatus) {
    for (uint8_t i = 0; i < SERVICE_ID_ALL; i++) {
        getService(i)->enable     = servMenu->toggle[i];
        termStorage->serviceEn[i] = servMenu->toggle[i];
    }
    SM_GOTO(state->parent);
}

STATE_DEF_ENTER(EnableServices) {
    ui_menu_togglable(servMenu, disp()->screen);
    for (uint8_t i = 0; i < SERVICE_ID_ALL; i++) {
        ui_menu_add_on_off_item(servMenu, getService(i)->state.name,
                                getService(i)->enable, NULL, NULL, NULL);
    }

    GOTO_MENU(saveServiceStatus, servMenu, NULL, NULL);
}

/*********************** other project state **************************/

static Menu* otherMenu;

static void saveSettings() { settings()->save(); }

STATE_DEF_ENTER(OtherProjects) {
    ui_menu_create(otherMenu, disp()->screen);
    for (uint8_t i = 0; i < OTH_PROJ_ALL; i++) {
        ui_menu_addItem(otherMenu, phraseGetDef(dsc[i]), subState[i], NULL,
                        NULL);
    }
    GOTO_MENU(state->parent, otherMenu, saveSettings, NULL);
}

OOP_CTOR(OtherProjects, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, "Other Projects");
    self->base.vtable.enter = STATE_ENTER(OtherProjects);

    subState[OTH_PROJ_FIXED_AMONT] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_FIXED_AMONT], self, "fixed amount");
    subState[OTH_PROJ_FIXED_AMONT]->vtable.enter = STATE_ENTER(FixedAmount);

    subState[OTH_PROJ_MAX_AMNT] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_MAX_AMNT], self, "max amount");
    subState[OTH_PROJ_MAX_AMNT]->vtable.enter = STATE_ENTER(MaxAmount);

    subState[OTH_PROJ_DIRECT_SALE] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_DIRECT_SALE], self, "direct sale");
    subState[OTH_PROJ_DIRECT_SALE]->vtable.enter = STATE_ENTER(DirectSale);

    subState[OTH_PROJ_EN_SERVICES] = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subState[OTH_PROJ_EN_SERVICES], self,
                  "enable services");
    subState[OTH_PROJ_EN_SERVICES]->vtable.enter = STATE_ENTER(EnableServices);

    enterMaxAmnt = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterMaxAmnt, self, "enter max amount");
    enterMaxAmnt->vtable.enter = STATE_ENTER(EnterMaxAmnt);

    getMaxAmnt = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, getMaxAmnt, self, "get max amount");
    getMaxAmnt->vtable.enter = STATE_ENTER(GetMaxAmnt);

    saveServiceStatus = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, saveServiceStatus, self, "save service status");
    saveServiceStatus->vtable.enter = STATE_ENTER(SaveServiceStatus);

    enterAmount = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterAmount, subState[OTH_PROJ_FIXED_AMONT],
                  "enter fixed amount");
    enterAmount->vtable.enter = STATE_ENTER(EnterFixedAmount);
    termStorage               = &settings()->terminal;

    fixedAmntMenu = MEM_ALLOC(sizeof(*fixedAmntMenu));
    maxAmntMenu   = MEM_ALLOC(sizeof(*maxAmntMenu));
    dirSaleMenu   = MEM_ALLOC(sizeof(*dirSaleMenu));
    otherMenu     = MEM_ALLOC(sizeof(*otherMenu));

    servMenu = MEM_ALLOC(sizeof(*servMenu));
}