#include "supervisor.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "ui/menu.h"
#include "sys/sys.h"
#include "storage/storage.h"
#include "utility/utility.h"
#include "common.h"
#include "settings/settings.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "input/inputMgr.h"

typedef enum {
    SUBS_NET_SETTINGS = 0,
    SUBS_KEY_INJECTION,
    SUBS_CONFIGURATION,
    SUBS_FARA,
    SUBS_MERCHANT_PASS_RESET,
    SUBS_UPDATE_APP,
    SUBS_DEFAULT_SETTINGS,
    SUBS_ALL
} SubStates_t;

static const char* itemTxt[SUBS_ALL] = {
    PHRASE_NETWORK_SETTINGS,  PHRASE_KEY_INJECTION,
    PHRASE_CONFIGURATION,     PHRASE_FARA,
    PHRASE_RESET_MERCHAT_PIN, PHRASE_UPDATE_APP,
    PHRASE_DEFAULT_SETTINGS};

static SubState* subStates[SUBS_ALL];
static SubState* enterPass;
static SubState* supervisorMenu;
static SubState* changeMerPin;

static bool validatePass(char* pass0, char* pass1, uint8_t len) {
    for (size_t i = 0; i < len; i++) {
        if (pass0[i] != pass1[i]) {
            return false;
        }
    }
    return true;
}

/******************** Enter pass sub state **********************/
static SubState* checkPass;

STATE_DEF_ENTER(CheckPassword) {
    bool isPassVlaid = validatePass("123456789", inmgr()->input, 4);
    if (isPassVlaid) {
        SM_GOTO(supervisorMenu);
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
            .title  = phraseGetDef(PHRASE_ENTER_PIN),
            .info   = "",
            .maxLen = PASSWORD_MAX_LEN,
        },
        STATE_IDLE, checkPass);
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
        GOTO_INFO(supervisorMenu, supervisorMenu, INFO_ERROR,
                  phraseGetDef(PHRASE_INCORRECT_PASSWORD), "");
    }
}

STATE_DEF_ENTER(EnterNewPin) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_PIN,
            .title  = PHRASE_NEW_PIN,
            .info   = PHRASE_NONE,
            .maxLen = PASSWORD_MAX_LEN,
        },
        state->parent, reEnterNewPin);
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
            .maxLen = PASSWORD_MAX_LEN,
        },
        supervisorMenu, checkNewPin);
    // GOTO_INPUT(supervisorMenu, checkNewPin,
    // phraseGetDef(PHRASE_REPEAT_NEW_PIN),
    //            "", 4, IN_MODE_PASSWORD, NULL);
}

STATE_DEF_ENTER(CheckNewPin) {
    bool isPassVlaid = validatePass(newPin, inmgr()->input, 4);
    if (isPassVlaid) {
        for (size_t i = 0; i < 4; i++) {
            settings()->terminal.merchantPin[i] = newPin[i];
        }
        settings()->save();
        GOTO_INFO(supervisorMenu, supervisorMenu, INFO_SUCCESS,
                  phraseGetDef(PHRASE_PIN_CHANGED_SUC), "");
    } else {
        GOTO_INFO(supervisorMenu, supervisorMenu, INFO_ERROR,
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
            .maxLen = PASSWORD_MAX_LEN,
        },
        state->parent, checkPin);
    // GOTO_INPUT(state->parent, checkPin, phraseGetDef(PHRASE_CURRENT_PIN), "",
    // 4,
    //            IN_MODE_PASSWORD, NULL);
}

static void ChangeMerPin(State* parent) {
    // subStates[SUBS_CHANGE_MERCHANT_PIN] = (SubState
    // *)MEM_ALLOC(sizeof(SubState)); OOP_CALL_CTOR(State,
    // subStates[SUBS_CHANGE_MERCHANT_PIN], parent, "change merchant pin");
    // subStates[SUBS_CHANGE_MERCHANT_PIN]->vtable.enter =
    // STATE_ENTER(ChangeMerPin);

    // checkPin = (SubState *)MEM_ALLOC(sizeof(SubState));
    // OOP_CALL_CTOR(State, checkPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
    // "change merchant pin"); checkPin->vtable.enter = STATE_ENTER(CheckPin);

    // enterNewPin = (SubState *)MEM_ALLOC(sizeof(SubState));
    // OOP_CALL_CTOR(State, enterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
    // "enter new merchant pin"); enterNewPin->vtable.enter =
    // STATE_ENTER(EnterNewPin);

    // reEnterNewPin = (SubState *)MEM_ALLOC(sizeof(SubState));
    // OOP_CALL_CTOR(State, reEnterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
    // "re enter new merchant pin"); reEnterNewPin->vtable.enter =
    // STATE_ENTER(ReEnterNewPin);

    // checkNewPin = (SubState *)MEM_ALLOC(sizeof(SubState));
    // OOP_CALL_CTOR(State, checkNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN],
    // "check new merchant pin"); checkNewPin->vtable.enter =
    // STATE_ENTER(CheckNewPin);
}

/******************** Server settings sub state **********************/

static Menu*     serverSetMenu;
static SubState* enterIp;
static SubState* enterPort;
static SubState* enableSsl;
static SubState* enterServerId;
static SubState* getServerId;
static SubState* success;
static Menu*     sslMenu;

typedef enum {
    SERV_SET_MAIN = 0,
    SERV_SET_TMS,
    SERV_SET_SSL,
    SERV_SET_ALL
} ServerSetItemt_t;

static ServerSetItemt_t serverItem;
static char             ip[24];
static uint16_t         port;
static uint16_t         serverId;

STATE_DEF_ENTER(EnterIp) {
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_PIN,
            .title  = phraseGetDef(PHRASE_ENTER_SERV_IP),
            .info   = "",
            .maxLen = IP_MAX_LEN,
        },
        state->parent, enterPort);
    // GOTO_INPUT(state->parent, enterPort, phraseGetDef(PHRASE_ENTER_SERV_IP),
    // "",
    //            IP_MAX_LEN, IN_MODE_IP, NULL);
    Input* in = STATE_INPUT;
    if (serverItem == SERV_SET_MAIN) {
        in->setInput(settings()->server.mainServerIp);
    } else if (serverItem == SERV_SET_TMS) {
        in->setInput(settings()->server.tmsIp);
    }
}

STATE_DEF_ENTER(EnterPort) {
    Input* in = STATE_INPUT;
    snprintf(ip, sizeof(ip), "%s", in->input);
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_NUMBERS,
            .title  = phraseGetDef(PHRASE_ENTER_SERV_PORT),
            .info   = "",
            .maxLen = 4,
        },
        state->parent, enterServerId);
    // GOTO_INPUT(state->parent, enterServerId,
    //            phraseGetDef(PHRASE_ENTER_SERV_PORT), "", 4, IN_MODE_NUMBERS,
    //            NULL);
    char str[5];
    if (serverItem == SERV_SET_MAIN) {
        intToStr(settings()->server.mainServerPort, str, sizeof(str));
    } else if (serverItem == SERV_SET_TMS) {
        intToStr(settings()->server.tmsPort, str, sizeof(str));
    }
    in->setInput(str);
}

STATE_DEF_ENTER(EnterServerId) {
    Input* in = STATE_INPUT;
    port      = toInt(in->input);
    inmgr()->run(
        &(InputCfg){
            .type   = INPUT_TYPE_KEYPAD,
            .mode   = INMD_ENTER_NUMBERS,
            .title  = phraseGetDef(PHRASE_ENTER_SERV_ID),
            .info   = "",
            .maxLen = 4,
        },
        state->parent, getServerId);
    // GOTO_INPUT(state->parent, getServerId,
    // phraseGetDef(PHRASE_ENTER_SERV_ID),
    //            "", 4, IN_MODE_NUMBERS, NULL);
    char str[5];
    if (serverItem == SERV_SET_MAIN) {
        intToStr(settings()->server.mainServerNii, str, sizeof(str));
    } else if (serverItem == SERV_SET_TMS) {
        intToStr(settings()->server.tmsId, str, sizeof(str));
    }
    in->setInput(str);
}

STATE_DEF_ENTER(GetServerId) {
    Input* in = STATE_INPUT;
    serverId  = toInt(in->input);
    SM_GOTO(success);
}

static void enSSL() { settings()->server.sslEn = 1; }

static void disSSL() { settings()->server.sslEn = 0; }

STATE_DEF_ENTER(EnableSsl) {
    ui_menu_create(sslMenu, disp()->screen);
    sslMenu->checkEnable = true;
    ui_menu_addItem(sslMenu, phraseGetDef(PHRASE_ENABLE), success, enSSL, NULL);
    ui_menu_addItem(sslMenu, phraseGetDef(PHRASE_DISABLE), success, disSSL,
                    NULL);
    GOTO_MENU(state->parent, sslMenu, NULL, NULL);
    ui_menu_set_checked(sslMenu, !settings()->server.sslEn);
}

STATE_DEF_ENTER(Success) {
    if (serverItem == SERV_SET_MAIN) {
        snprintf(settings()->server.mainServerIp,
                 sizeof(settings()->server.mainServerIp), "%s", ip);
        settings()->server.mainServerPort = port;
        settings()->server.mainServerNii  = serverId;
    } else if (serverItem == SERV_SET_TMS) {
        snprintf(settings()->server.tmsIp, sizeof(settings()->server.tmsIp),
                 "%s", ip);
        settings()->server.tmsPort = port;
        settings()->server.tmsId   = serverId;
    }
    settings()->save();
    GOTO_INFO(state->parent, state->parent, INFO_SUCCESS,
              phraseGetDef(PHRASE_SUC_DONME), "");
}

static void setItemToMainServer() { serverItem = SERV_SET_MAIN; }

static void setItemToTms() { serverItem = SERV_SET_TMS; }

STATE_DEF_ENTER(NetworkSettings) {
    ui_menu_create(serverSetMenu, disp()->screen);
    ui_menu_addItem(serverSetMenu, phraseGetDef(PHRASE_SERVER_SETTINGS),
                    enterIp, setItemToMainServer, NULL);
    ui_menu_addItem(serverSetMenu, phraseGetDef(PHRASE_TMS_SETTINGS), enterIp,
                    setItemToTms, NULL);
    ui_menu_addItem(serverSetMenu, phraseGetDef(PHRASE_SSL), enableSsl, NULL,
                    NULL);
    GOTO_MENU(state->parent, serverSetMenu, NULL, NULL);
}

OOP_CTOR(NetworkSettings, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(NetworkSettings);

    enterIp = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterIp, self, "enter ip");
    enterIp->vtable.enter = STATE_ENTER(EnterIp);

    enterPort = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPort, self, "enter port");
    enterPort->vtable.enter = STATE_ENTER(EnterPort);

    enableSsl = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enableSsl, self, "enable ssl");
    enableSsl->vtable.enter = STATE_ENTER(EnableSsl);

    enterServerId = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, enterServerId, self, "enter server id");
    enterServerId->vtable.enter = STATE_ENTER(EnterServerId);

    success = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, success, self, "sucess server settings");
    success->vtable.enter = STATE_ENTER(Success);

    getServerId = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, getServerId, self, "get server id");
    getServerId->vtable.enter = STATE_ENTER(GetServerId);
}

/******************** Merchant pass reset sub state **********************/

STATE_DEF_ENTER(MerchantPassReset) {
    snprintf(settings()->terminal.merchantPin, MERCHANT_PIN_LEN + 1, "%s",
             MERCHANT_DEFAULT_PIN);
    settings()->save();
    GOTO_INFO(state->parent, state->parent, INFO_SUCCESS,
              phraseGetDef(PHRASE_SUC_DONME), "");
}

OOP_CTOR(MerchantPassReset, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(MerchantPassReset);
}

/******************** FARA sub state **********************/

STATE_DEF_ENTER(FARA) { GOTO_DEV_INFO(state->parent); }

OOP_CTOR(FARA, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(FARA);
}

/******************** Update app sub state **********************/

STATE_DEF_ENTER(UpdateApp) { GOTO_DEV_INFO(state->parent); }

OOP_CTOR(UpdateApp, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(UpdateApp);
}

/******************** Default Settings sub state **********************/

STATE_DEF_ENTER(DefaultSettings) {
    settings()->reset();
    GOTO_INFO(state->parent, state->parent, INFO_SUCCESS,
              phraseGetDef(PHRASE_SUC_DONME), "");
}

OOP_CTOR(DefaultSettings, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(DefaultSettings);
}

/******************** Supervisor menu sub state **********************/

static Menu* menu;

STATE_DEF_ENTER(SupervisorMenu) {
    ui_menu_create(menu, disp()->screen);
    for (uint8_t i = 0; i < SUBS_ALL; i++) {
        ui_menu_addItem(menu, phraseGetDef(itemTxt[i]), subStates[i], NULL,
                        NULL);
    }
    GOTO_MENU(getState(STATE_ID_SUPPORTER), menu, NULL, NULL);
}

static void SupervisorMenu(State* parent) {
    supervisorMenu = (SubState*)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, supervisorMenu, parent, "supervisor menu");
    supervisorMenu->vtable.enter = STATE_ENTER(SupervisorMenu);
}

/******************************************************************/

STATE_DEF_ENTER(Supervisor) { SM_GOTO(enterPass); }

STATE_DEF_EXIT(Supervisor) {}

OOP_CTOR(Supervisor, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Supervisor);
    self->base.vtable.exit  = STATE_EXIT(Supervisor);

    EnterPassword(self);
    SupervisorMenu(self);
    ChangeMerPin(self);

    subStates[SUBS_NET_SETTINGS] =
        (NetworkSettings*)MEM_ALLOC(sizeof(NetworkSettings));
    OOP_CALL_CTOR(NetworkSettings, subStates[SUBS_NET_SETTINGS], supervisorMenu,
                  "terminal settings");
    subStates[SUBS_KEY_INJECTION] =
        (KeyInjection*)MEM_ALLOC(sizeof(KeyInjection));
    OOP_CALL_CTOR(KeyInjection, subStates[SUBS_KEY_INJECTION], supervisorMenu,
                  "key injection");
    subStates[SUBS_CONFIGURATION] =
        (Configuration*)MEM_ALLOC(sizeof(Configuration));
    OOP_CALL_CTOR(Configuration, subStates[SUBS_CONFIGURATION], supervisorMenu,
                  "configuration");
    subStates[SUBS_FARA] = (FARA*)MEM_ALLOC(sizeof(FARA));
    OOP_CALL_CTOR(FARA, subStates[SUBS_FARA], supervisorMenu, "FARA");
    subStates[SUBS_MERCHANT_PASS_RESET] =
        (MerchantPassReset*)MEM_ALLOC(sizeof(MerchantPassReset));
    OOP_CALL_CTOR(MerchantPassReset, subStates[SUBS_MERCHANT_PASS_RESET],
                  supervisorMenu, "merchant pass reset");
    subStates[SUBS_UPDATE_APP] = (UpdateApp*)MEM_ALLOC(sizeof(UpdateApp));
    OOP_CALL_CTOR(UpdateApp, subStates[SUBS_UPDATE_APP], supervisorMenu,
                  "update app");
    subStates[SUBS_DEFAULT_SETTINGS] =
        (DefaultSettings*)MEM_ALLOC(sizeof(DefaultSettings));
    OOP_CALL_CTOR(DefaultSettings, subStates[SUBS_DEFAULT_SETTINGS],
                  supervisorMenu, "default settings");

    sslMenu       = MEM_ALLOC(sizeof(*sslMenu));
    serverSetMenu = MEM_ALLOC(sizeof(*serverSetMenu));
    menu          = MEM_ALLOC(sizeof(*menu));
}