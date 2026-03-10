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

STATE_DEF_ENTER(FixedAmount) {
    GOTO_INFO(state->parent, state->parent, "not implemented yet", "");
}

/******************** max amount sub state **********************/
static Menu maxAmntMenu;
static SubState *enterMaxAmnt;
static SubState *getMaxAmnt;

static void disMaxAmnt() {
    getStorage()->settings->terminal.maxAmntEnable = false;
    OOP_CALL(getStorage(), applySettings);
}

STATE_DEF_ENTER(GetMaxAmnt) {
    Input *in = getState(STATE_ID_INPUT);
    GOTO_INFO(state->parent, state->parent, "با موفقیت فعال شد", "");
    getStorage()->settings->terminal.maxAmntEnable = true;
    snprintf(getStorage()->settings->terminal.maxAmnt,
        13, "%s", in->input);
    OOP_CALL(getStorage(), applySettings);
}

STATE_DEF_ENTER(EnterMaxAmnt) {
    GOTO_INPUT(state->parent, getMaxAmnt, "سقف مبلغ", "(ریال)", 12, IN_MODE_AMOUNT);
    in->setAmount(getStorage()->settings->terminal.maxAmnt);
}

STATE_DEF_ENTER(MaxAmount) {
    uiMenu(&maxAmntMenu, getDisplay()->screen);
    maxAmntMenu.checkEnable = true;
    OOP_CALL(&maxAmntMenu, addItem, "فعال", enterMaxAmnt, NULL, NULL);
    OOP_CALL(&maxAmntMenu, addItem, "غیر فعال", NULL, disMaxAmnt, NULL);
    if(getStorage()->settings->terminal.maxAmntEnable) {
        OOP_CALL(&maxAmntMenu, setChecked, 0);
    } else {
        OOP_CALL(&maxAmntMenu, setChecked, 1);
    }
    GOTO_MENU(state->parent, &maxAmntMenu);
}

/******************** direct sale sub state **********************/
static Menu dirSaleMenu;

static void enDirectSale() {
    getStorage()->settings->terminal.maxAmntEnable = false;
    OOP_CALL(getStorage(), applySettings);
}

static void disDirectSale() {
    getStorage()->settings->terminal.maxAmntEnable = false;
    OOP_CALL(getStorage(), applySettings);
}

STATE_DEF_ENTER(DirectSale) {
    uiMenu(&dirSaleMenu, getDisplay()->screen);
    dirSaleMenu.checkEnable = true;
    OOP_CALL(&dirSaleMenu, addItem, "فعال", NULL, enDirectSale, NULL);
    OOP_CALL(&dirSaleMenu, addItem, "غیر فعال", NULL, disDirectSale, NULL);

    GOTO_MENU(state->parent, &dirSaleMenu);
}

/******************** enable services sub state **********************/

static Menu servMenu;
static SubState *saveServiceStatus;

STATE_DEF_ENTER(SaveServiceStatus) {
    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        getService(i)->enable = servMenu.toggle[i];
        getStorage()->settings->terminal.serviceEn[i] = servMenu.toggle[i];
    }
    OOP_CALL(getStorage(), applySettings);
    SM_GOTO(state->parent);
}

STATE_DEF_ENTER(EnableServices) {
    uiToggleMenu(&servMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        OOP_CALL(&servMenu, addOnOffItem, getService(i)->state.name,
            getService(i)->enable, NULL, NULL, NULL);
    }

    GOTO_MENU(saveServiceStatus, &servMenu);
}

/*********************** other project state **************************/

static Menu otherMenu;

STATE_DEF_ENTER(OtherProjects) {
    uiMenu(&otherMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < OTH_PROJ_ALL ; i++) {
        OOP_CALL(&otherMenu, addItem, otherItemTxt[i], subState[i], NULL, NULL);
    }
    GOTO_MENU(state->parent, &otherMenu);
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
}