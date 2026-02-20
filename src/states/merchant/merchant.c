#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "ui/ui.h"
#include "dev/dev.h"

typedef enum {
    SUBS_REPORTS = 0,
    SUBS_SHIFT,
    SUBS_MERCHANT_DATA,
    SUBS_SPEC_PROJECTS,
    SUBS_SETTINGS,
    SUBS_CONNECTIONS,
    SUBS_ALL
} SubStates_t;

static SubState *subStates[SUBS_ALL];

static SubState *enterPass;
static SubState *merchantMenu;

#define PASSWORD_MAX_LEN 4

STATE_DEF_ENTER(Merchant) {
    SM_GOTO(enterPass);
}

STATE_DEF_EXIT(Merchant) {
}

/******************** Enter pass sub state **********************/

STATE_DEF_ENTER(EnterPassword) {
    Input * in = (Input*)getState(STATE_ID_INPUT);
    in->reset();
    in->setMode(IN_MODE_PASSWORD);
    in->setData("ورود رمز", "");
    in->setMax(PASSWORD_MAX_LEN);
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, getState(STATE_ID_IDLE));
    OOP_CALL(getState(STATE_ID_INPUT), setNext, merchantMenu);
    SM_GOTO(getState(STATE_ID_INPUT));
}

STATE_DEF_EXIT(EnterPassword) {

}

static void EnterPassword(State *parent) {
    enterPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, parent, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);
    enterPass->vtable.exit = STATE_EXIT(EnterPassword);
}

static void D_EnterPassword() {
    FREE_MEM(enterPass);
}

/******************** Merchant menu sub state **********************/

static const char* itemTxt[SUBS_ALL] = {
    "گزارش",
    "شیفت کاری",
    "اطلاعات پذیرنده",
    "پروژه های خاص",
    "تنظیمات",
    "بستر ارتباطی",
};

static Menu menu;

static void createUi() {
    uiMenu(&menu, getDisplay()->screen);
    for (uint8_t i = 0; i < SUBS_ALL ; i++) {
        OOP_CALL(&menu, addItem, itemTxt[i], NULL, NULL);
    }
}

static void destroyUi() {
    uiDeleteMenu(&menu);
}

STATE_DEF_ENTER(MerchantMenu) {
    createUi();
    OOP_CALL(&menu, show);
}

STATE_DEF_EXIT(MerchantMenu) {
    OOP_CALL(&menu, hide);
    destroyUi();
}

static void handleKeyAction(State *state, int id) {
    if (id >= SUBS_ALL) {
        return;
    }
    SM_GOTO(subStates[id]);
}

STATE_DEF_HANDLE(MerchantMenu, KeypadEvent) {
    OOP_CALL(&menu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(getState(STATE_ID_SUPPORTER));
    } else if (ev->key == KEY_ENTER) {
        handleKeyAction(state, menu.idx);
    }  else {
        if (ev->key <= KEY_9) {
            int id = ((int)ev->key - 1);
            handleKeyAction(state, id);
        }
    }
}

static void MerchantMenu(State *parent) {
    merchantMenu = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, merchantMenu, parent, "merchant menu");
    merchantMenu->vtable.enter = STATE_ENTER(MerchantMenu);
    merchantMenu->vtable.exit = STATE_EXIT(MerchantMenu);
    merchantMenu->vtable.handleKeypad = STATE_HANDLE(MerchantMenu, KeypadEvent);
}

/******************************************************************/

OOP_CTOR(Merchant, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Merchant);
    self->base.vtable.exit = STATE_EXIT(Merchant);
    EnterPassword(self);
    MerchantMenu(self);
    subStates[SUBS_CONNECTIONS] = (Connections *)GET_MEM(sizeof(Connections));
    OOP_CALL_CTOR(Connections, subStates[SUBS_CONNECTIONS], merchantMenu, "connections");
    subStates[SUBS_SETTINGS] = (Settings *)GET_MEM(sizeof(Settings));
    OOP_CALL_CTOR(Settings, subStates[SUBS_SETTINGS], merchantMenu, "settings");

}