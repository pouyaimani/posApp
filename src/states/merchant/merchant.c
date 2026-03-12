#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "storage/storage.h"

static Storage *storage;

static bool validatePass(char *pass0, char *pass1, uint8_t len) {
    for (size_t i = 0; i < len ; i++) {
        if (pass0[i] != pass1[i]) {
            return false;
        }
    }
    return true;
}

typedef enum {
    SUBS_REPORTS = 0,
    SUBS_SHIFT,
    SUBS_MERCHANT_DATA,
    SUBS_OTHER_PROJECTS,
    SUBS_SETTINGS,
    SUBS_CONNECTIONS,
    SUBS_CHANGE_MERCHANT_PIN,
    SUBS_ALL
} SubStates_t;

static SubState *subStates[SUBS_ALL];

static SubState *enterPass;
static SubState *merchantMenu;
static SubState *changeMerPin;

#define PASSWORD_MAX_LEN 4

STATE_DEF_ENTER(Merchant) {
    SM_GOTO(enterPass);
}

STATE_DEF_EXIT(Merchant) {
}

/******************** Enter pass sub state **********************/
static SubState *checkPass;

STATE_DEF_ENTER(CheckPassword) {
    Input *in = getState(STATE_ID_INPUT);
    bool isPassVlaid = validatePass(storage->settings->terminal.operatePwd,
                in->password, 4);
    if (isPassVlaid) {
        SM_GOTO(merchantMenu);
    } else {
        GOTO_INFO(getState(STATE_ID_SUPPORTER), getState(STATE_ID_SUPPORTER), "رمز عبور نادرست است", "");
    }
}

STATE_DEF_ENTER(EnterPassword) {
    GOTO_INPUT(STATE_IDLE, checkPass,
        "ورود رمز", "", PASSWORD_MAX_LEN, IN_MODE_PASSWORD);
}

static void EnterPassword(State *parent) {
    enterPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, parent, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    checkPass = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, checkPass, parent, "check password");
    checkPass->vtable.enter = STATE_ENTER(CheckPassword);
}

static void D_EnterPassword() {
    FREE_MEM(enterPass);
}

/******************** change merchant pin sub state **********************/
static SubState *checkPin;
static SubState *enterNewPin;
static SubState *reEnterNewPin;
static SubState *checkNewPin;

static char newPin[4 + 1];

STATE_DEF_ENTER(CheckPin) {
    Input *in = getState(STATE_ID_INPUT);
    bool isPassVlaid = validatePass(storage->settings->terminal.operatePwd,
                in->password, 4);
    if (isPassVlaid) {
        SM_GOTO(enterNewPin);
    } else {
        GOTO_INFO(merchantMenu, merchantMenu, "رمز نادرست است", "");
    }
}

STATE_DEF_ENTER(EnterNewPin) {
    GOTO_INPUT(state->parent, reEnterNewPin, "رمز جدید", "", 4, IN_MODE_PASSWORD);
}

STATE_DEF_ENTER(ReEnterNewPin) {
    Input *inp = getState(STATE_ID_INPUT);
    for (size_t i = 0; i < 4; i++) {
        newPin[i] = inp->password[i];
    }
    GOTO_INPUT(merchantMenu, checkNewPin, "تکرار رمز جدید", "", 4, IN_MODE_PASSWORD);
}

STATE_DEF_ENTER(CheckNewPin) {
    Input *in = getState(STATE_ID_INPUT);
    bool isPassVlaid = validatePass(newPin,
                in->password, 4);
    if(isPassVlaid) {
        for (size_t i = 0; i < 4; i++) {
            storage->settings->terminal.operatePwd[i] = newPin[i];
        }
        SAVE_SETTINGS();
        GOTO_INFO(merchantMenu, merchantMenu, "رمز با موفقیت تغییر کرد", "");
    } else {
        GOTO_INFO(merchantMenu, merchantMenu, "تاییدیه رمز نادرست است", "");
    }
}

STATE_DEF_ENTER(ChangeMerPin) {
    GOTO_INPUT(state->parent, checkPin, "رمز فعلی", "", 4, IN_MODE_PASSWORD);
}

static void ChangeMerPin(State *parent) {
    subStates[SUBS_CHANGE_MERCHANT_PIN] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subStates[SUBS_CHANGE_MERCHANT_PIN], parent, "change merchant pin");
    subStates[SUBS_CHANGE_MERCHANT_PIN]->vtable.enter = STATE_ENTER(ChangeMerPin);

    checkPin = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, checkPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "change merchant pin");
    checkPin->vtable.enter = STATE_ENTER(CheckPin);

    enterNewPin = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "enter new merchant pin");
    enterNewPin->vtable.enter = STATE_ENTER(EnterNewPin);

    reEnterNewPin = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, reEnterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "re enter new merchant pin");
    reEnterNewPin->vtable.enter = STATE_ENTER(ReEnterNewPin);

    checkNewPin = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, checkNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "check new merchant pin");
    checkNewPin->vtable.enter = STATE_ENTER(CheckNewPin);

}

/******************** Merchant data sub state **********************/

STATE_DEF_ENTER(MerchantData) {
    GOTO_DEV_INFO(state->parent);
}

OOP_CTOR(MerchantData, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, "Merchant Data");
    self->base.vtable.enter = STATE_ENTER(MerchantData);
}

/******************** Merchant menu sub state **********************/

static const char* itemTxt[SUBS_ALL] = {
    "گزارش",
    "شیفت کاری",
    "اطلاعات پذیرنده",
    "پروژه های خاص",
    "تنظیمات",
    "بستر ارتباطی",
    "تغییر رمز پذیرنده"
};

static Menu menu;

static void createUi() {
    uiMenu(&menu, getDisplay()->screen);
    for (uint8_t i = 0; i < SUBS_ALL ; i++) {
        OOP_CALL(&menu, addItem, itemTxt[i], subStates[i], NULL, NULL);
    }
}

STATE_DEF_ENTER(MerchantMenu) {
    createUi();
    GOTO_MENU(getState(STATE_ID_SUPPORTER), &menu, NULL);
}

static void MerchantMenu(State *parent) {
    merchantMenu = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, merchantMenu, parent, "merchant menu");
    merchantMenu->vtable.enter = STATE_ENTER(MerchantMenu);
}

/******************************************************************/

OOP_CTOR(Merchant, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Merchant);
    self->base.vtable.exit = STATE_EXIT(Merchant);
    EnterPassword(self);
    MerchantMenu(self);
    ChangeMerPin(self);
    subStates[SUBS_CONNECTIONS] = (Connections *)GET_MEM(sizeof(Connections));
    OOP_CALL_CTOR(Connections, subStates[SUBS_CONNECTIONS], merchantMenu, "connections");
    subStates[SUBS_SETTINGS] = (Settings *)GET_MEM(sizeof(Settings));
    OOP_CALL_CTOR(Settings, subStates[SUBS_SETTINGS], merchantMenu, "settings");
    subStates[SUBS_REPORTS] = (Reports *)GET_MEM(sizeof(Reports));
    OOP_CALL_CTOR(Reports, subStates[SUBS_REPORTS], merchantMenu, "reports");
    subStates[SUBS_SHIFT] = (Shift *)GET_MEM(sizeof(Shift));
    OOP_CALL_CTOR(Shift, subStates[SUBS_SHIFT], merchantMenu, "shift");
    subStates[SUBS_OTHER_PROJECTS] = (OtherProjects *)GET_MEM(sizeof(OtherProjects));
    OOP_CALL_CTOR(OtherProjects, subStates[SUBS_OTHER_PROJECTS], merchantMenu, "other projects");
    subStates[SUBS_MERCHANT_DATA] = (MerchantData *)GET_MEM(sizeof(MerchantData));
    OOP_CALL_CTOR(MerchantData, subStates[SUBS_MERCHANT_DATA], merchantMenu, "merchant data");

    storage = getStorage();
}