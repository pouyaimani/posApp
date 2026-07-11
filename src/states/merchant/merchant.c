#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "ui/menu.h"
#include "sys/sys.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "input/inputMgr.h"

static bool validatePass(char* pass0, char* pass1, uint8_t len) {
    for (size_t i = 0; i < len; i++) {
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

static SubState* subStates[SUBS_ALL];

static SubState* enterPass;
static SubState* merchantMenu;
static SubState* changeMerPin;

STATE_DEF_ENTER(Merchant) { SM_GOTO(enterPass); }

STATE_DEF_EXIT(Merchant) {}

/******************** Enter pass sub state **********************/
static SubState* checkPass;

STATE_DEF_ENTER(CheckPassword) {
    bool isPassVlaid =
        validatePass(settings()->terminal.merchantPin, inmgr()->input, 4);
    if (isPassVlaid) {
        SM_GOTO(merchantMenu);
    } else {
        GOTO_INFO(getState(STATE_ID_SUPPORTER), getState(STATE_ID_SUPPORTER),
                  INFO_ERROR, phraseGetDef(PHRASE_INCORRECT_PASSWORD), "");
    }
}

STATE_DEF_ENTER(EnterPassword) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_PASS_ENTRY),
            .info   = "",
            .maxLen = LEN_MAX_PASSWORD,
        },
        STATE_IDLE, checkPass);
    // GOTO_INPUT(STATE_IDLE, checkPass, phraseGetDef(PHRASE_PASS_ENTRY), "",
    //            LEN_MAX_PASSWORD, IN_MODE_PASSWORD, NULL);
}

static void EnterPassword(State* parent) {
    enterPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPass, parent, "enter password");
    enterPass->vtable.enter = STATE_ENTER(EnterPassword);

    checkPass = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, checkPass, parent, "check password");
    checkPass->vtable.enter = STATE_ENTER(CheckPassword);
}

static void D_EnterPassword() { MEM_FREE(enterPass); }

/******************** change merchant pin sub state **********************/
static SubState* checkPin;
static SubState* enterNewPin;
static SubState* reEnterNewPin;
static SubState* checkNewPin;

static char newPin[4 + 1];

STATE_DEF_ENTER(CheckPin) {
    bool isPassVlaid =
        validatePass(settings()->terminal.merchantPin, inmgr()->input, 4);
    if (isPassVlaid) {
        SM_GOTO(enterNewPin);
    } else {
        GOTO_INFO(merchantMenu, merchantMenu, INFO_ERROR,
                  phraseGetDef(PHRASE_INCORRECT_PASSWORD), "");
    }
}

STATE_DEF_ENTER(EnterNewPin) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_NEW_PIN),
            .info   = "",
            .maxLen = LEN_MAX_PASSWORD,
        },
        state->parent, reEnterNewPin);
    // GOTO_INPUT(state->parent, reEnterNewPin, phraseGetDef(PHRASE_NEW_PIN),
    // "",
    //            4, IN_MODE_PASSWORD, NULL);
}

STATE_DEF_ENTER(ReEnterNewPin) {
    for (size_t i = 0; i < 4; i++) {
        newPin[i] = inmgr()->input[i];
    }
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_REPEAT_NEW_PIN),
            .info   = "",
            .maxLen = LEN_MAX_PASSWORD,
        },
        merchantMenu, checkNewPin);
    // GOTO_INPUT(merchantMenu, checkNewPin,
    // phraseGetDef(PHRASE_REPEAT_NEW_PIN),
    //            "", 4, IN_MODE_PASSWORD, NULL);
}

STATE_DEF_ENTER(CheckNewPin) {
    bool isPassVlaid = validatePass(newPin, inmgr()->input, 4);
    if (isPassVlaid) {
        snprintf(settings()->terminal.merchantPin, LEN_MERCHANT_PIN + 1, "%s",
                 newPin);
        settings()->save();
        GOTO_INFO(merchantMenu, merchantMenu, INFO_SUCCESS,
                  phraseGetDef(PHRASE_PIN_CHANGED_SUC), "");
    } else {
        GOTO_INFO(merchantMenu, merchantMenu, INFO_ERROR,
                  phraseGetDef(PHRASE_PIN_CONFIRM_ERR), "");
    }
}

STATE_DEF_ENTER(ChangeMerPin) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_CURRENT_PIN),
            .info   = "",
            .maxLen = LEN_MAX_PASSWORD,
        },
        state->parent, checkPin);
    // GOTO_INPUT(state->parent, checkPin, phraseGetDef(PHRASE_CURRENT_PIN), "",
    // 4,
    //            IN_MODE_PASSWORD, NULL);
}

static void ChangeMerPin(State* parent) {
    subStates[SUBS_CHANGE_MERCHANT_PIN] =
        (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subStates[SUBS_CHANGE_MERCHANT_PIN], parent,
                  "change merchant pin");
    subStates[SUBS_CHANGE_MERCHANT_PIN]->vtable.enter =
        STATE_ENTER(ChangeMerPin);

    checkPin = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, checkPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
                  "change merchant pin");
    checkPin->vtable.enter = STATE_ENTER(CheckPin);

    enterNewPin = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
                  "enter new merchant pin");
    enterNewPin->vtable.enter = STATE_ENTER(EnterNewPin);

    reEnterNewPin = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, reEnterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
                  "re enter new merchant pin");
    reEnterNewPin->vtable.enter = STATE_ENTER(ReEnterNewPin);

    checkNewPin = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, checkNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
                  "check new merchant pin");
    checkNewPin->vtable.enter = STATE_ENTER(CheckNewPin);
}

/******************** Merchant data sub state **********************/

STATE_DEF_ENTER(MerchantData) { GOTO_DEV_INFO(state->parent); }

OOP_CTOR(MerchantData, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, "Merchant Data");
    self->base.vtable.enter = STATE_ENTER(MerchantData);
}

/******************** Merchant menu sub state **********************/

static const Phrases_t dsc[SUBS_ALL] = {PHRASE_REPORT,
                                        PHRASE_SHIFT,
                                        PHRASE_MERCHANT_INFO,
                                        PHRASE_SPECEFIC_PROJECT,
                                        PHRASE_SETTINGS,
                                        PHRASE_CONNECTION,
                                        PHRASE_MERCHANT_PIN_CHANGE};

static Menu* menu;

static void createUi() {
    ui_menu_create(menu, disp()->screen);
    for (uint8_t i = 0; i < SUBS_ALL; i++) {
        ui_menu_addItem(menu, phraseGetDef(dsc[i]), subStates[i], NULL, NULL);
    }
}

STATE_DEF_ENTER(MerchantMenu) {
    createUi();
    GOTO_MENU(getState(STATE_ID_SUPPORTER), menu, NULL, NULL);
}

static void MerchantMenu(State* parent) {
    merchantMenu = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, merchantMenu, parent, "merchant menu");
    merchantMenu->vtable.enter = STATE_ENTER(MerchantMenu);
}

/******************************************************************/

OOP_CTOR(Merchant, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Merchant);
    self->base.vtable.exit  = STATE_EXIT(Merchant);
    EnterPassword(self);
    MerchantMenu(self);
    ChangeMerPin(self);
    subStates[SUBS_CONNECTIONS] = (Connections*)MEM_ALLOC(sizeof(Connections));
    OOP_CALL_CTOR(Connections, subStates[SUBS_CONNECTIONS], merchantMenu,
                  "connections");
    subStates[SUBS_SETTINGS] = (Settings*)MEM_ALLOC(sizeof(Settings));
    OOP_CALL_CTOR(Settings, subStates[SUBS_SETTINGS], merchantMenu, "settings");
    subStates[SUBS_REPORTS] = (Reports*)MEM_ALLOC(sizeof(Reports));
    OOP_CALL_CTOR(Reports, subStates[SUBS_REPORTS], merchantMenu, "reports");
    subStates[SUBS_SHIFT] = (Shift*)MEM_ALLOC(sizeof(Shift));
    OOP_CALL_CTOR(Shift, subStates[SUBS_SHIFT], merchantMenu, "shift");
    subStates[SUBS_OTHER_PROJECTS] =
        (OtherProjects*)MEM_ALLOC(sizeof(OtherProjects));
    OOP_CALL_CTOR(OtherProjects, subStates[SUBS_OTHER_PROJECTS], merchantMenu,
                  "other projects");
    subStates[SUBS_MERCHANT_DATA] =
        (MerchantData*)MEM_ALLOC(sizeof(MerchantData));
    OOP_CALL_CTOR(MerchantData, subStates[SUBS_MERCHANT_DATA], merchantMenu,
                  "merchant data");

    menu = MEM_ALLOC(sizeof(*menu));
}