#include "supervisor.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "ui/ui.h"
#include "sys/sys.h"
#include "storage/storage.h"
#include "utility/utility.h"
#include "common.h"
#include "settings/settings.h"
#include "phrases/phrases.h"

#define PASSWORD_MAX_LEN 4
#define IP_MAX_LEN 12

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
    PHRASE_NETWORK_SETTINGS,
    PHRASE_KEY_INJECTION,
    PHRASE_CONFIGURATION,
    PHRASE_FARA,
    PHRASE_RESET_MERCHAT_PIN,
    PHRASE_UPDATE_APP,
    PHRASE_DEFAULT_SETTINGS
};

static SubState *subStates[SUBS_ALL];
static SubState *enterPass;
static SubState *supervisorMenu;
static SubState *changeMerPin;

static bool validatePass(char *pass0, char *pass1, uint8_t len) {
    for (size_t i = 0; i < len ; i++) {
        if (pass0[i] != pass1[i]) {
            return false;
        }
    }
    return true;
}

/******************** Enter pass sub state **********************/
static SubState *checkPass;

STATE_DEF_ENTER(CheckPassword) {
    Input *in = getState(STATE_ID_INPUT);
    bool isPassVlaid = validatePass("123456789",
                in->password, 4);
    if (isPassVlaid) {
        SM_GOTO(supervisorMenu);
    } else {
        GOTO_INFO(getState(STATE_ID_SUPPORTER), getState(STATE_ID_SUPPORTER),
                 phraseGetDef(PHRASE_INCORRECT_PASSWORD), "");
    }
}

STATE_DEF_ENTER(EnterPassword) {
    GOTO_INPUT(STATE_IDLE, checkPass,
        phraseGetDef(PHRASE_ENTER_PIN), "", PASSWORD_MAX_LEN, IN_MODE_PASSWORD, NULL);
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
    bool isPassVlaid = validatePass(settings()->terminal.merchantPin,
                in->password, 4);
    if (isPassVlaid) {
        SM_GOTO(enterNewPin);
    } else {
        GOTO_INFO(supervisorMenu, supervisorMenu, phraseGetDef(PHRASE_INCORRECT_PASSWORD), "");
    }
}

STATE_DEF_ENTER(EnterNewPin) {
    GOTO_INPUT(state->parent, reEnterNewPin, 
            phraseGetDef(PHRASE_NEW_PIN), "", 4, IN_MODE_PASSWORD, NULL);
}

STATE_DEF_ENTER(ReEnterNewPin) {
    Input *inp = getState(STATE_ID_INPUT);
    for (size_t i = 0; i < 4; i++) {
        newPin[i] = inp->password[i];
    }
    GOTO_INPUT(supervisorMenu, checkNewPin, 
            phraseGetDef(PHRASE_REPEAT_NEW_PIN), "", 4, IN_MODE_PASSWORD, NULL);
}

STATE_DEF_ENTER(CheckNewPin) {
    Input *in = getState(STATE_ID_INPUT);
    bool isPassVlaid = validatePass(newPin,
                in->password, 4);
    if(isPassVlaid) {
        for (size_t i = 0; i < 4; i++) {
            settings()->terminal.merchantPin[i] = newPin[i];
        }
        settings()->save();
        GOTO_INFO(supervisorMenu, supervisorMenu, phraseGetDef(PHRASE_PIN_CHANGED_SUC), "");
    } else {
        GOTO_INFO(supervisorMenu, supervisorMenu, phraseGetDef(PHRASE_PIN_CONFIRM_ERR), "");
    }
}

STATE_DEF_ENTER(ChangeMerPin) {
    GOTO_INPUT(state->parent, checkPin, phraseGetDef(PHRASE_CURRENT_PIN), "", 4, IN_MODE_PASSWORD, NULL);
}

static void ChangeMerPin(State *parent) {
    // subStates[SUBS_CHANGE_MERCHANT_PIN] = (SubState *)GET_MEM(sizeof(SubState));
    // OOP_CALL_CTOR(State, subStates[SUBS_CHANGE_MERCHANT_PIN], parent, "change merchant pin");
    // subStates[SUBS_CHANGE_MERCHANT_PIN]->vtable.enter = STATE_ENTER(ChangeMerPin);

    // checkPin = (SubState *)GET_MEM(sizeof(SubState));
    // OOP_CALL_CTOR(State, checkPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "change merchant pin");
    // checkPin->vtable.enter = STATE_ENTER(CheckPin);

    // enterNewPin = (SubState *)GET_MEM(sizeof(SubState));
    // OOP_CALL_CTOR(State, enterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "enter new merchant pin");
    // enterNewPin->vtable.enter = STATE_ENTER(EnterNewPin);

    // reEnterNewPin = (SubState *)GET_MEM(sizeof(SubState));
    // OOP_CALL_CTOR(State, reEnterNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "re enter new merchant pin");
    // reEnterNewPin->vtable.enter = STATE_ENTER(ReEnterNewPin);

    // checkNewPin = (SubState *)GET_MEM(sizeof(SubState));
    // OOP_CALL_CTOR(State, checkNewPin, subStates[SUBS_CHANGE_MERCHANT_PIN], "check new merchant pin");
    // checkNewPin->vtable.enter = STATE_ENTER(CheckNewPin);

}

/******************** Server settings sub state **********************/

static Menu serverSetMenu;
static SubState *enterIp;
static SubState *enterPort;
static SubState *enableSsl;
static SubState *enterServerId;
static SubState *getServerId;
static SubState *success;
static Menu sslMenu;

typedef enum {
    SERV_SET_MAIN = 0,
    SERV_SET_TMS,
    SERV_SET_SSL,
    SERV_SET_ALL
} ServerSetItemt_t;

static ServerSetItemt_t serverItem;
static char ip[12 + 1];
static uint16_t port;
static uint16_t serverId;

static const char* serverSetTxt[SUBS_ALL] = {
    "تنظیمات سرور",
    "تنظیمات TMS",
    "SSL"
};

STATE_DEF_ENTER(EnterIp) {
    GOTO_INPUT(state->parent, enterPort, phraseGetDef(PHRASE_ENTER_SERV_IP),
        "", IP_MAX_LEN, IN_MODE_IP, NULL);
    Input * in = STATE_INPUT;
    if (serverItem == SERV_SET_MAIN) {
        in->setInput(settings()->server.mainServerIp);
    } else if (serverItem == SERV_SET_TMS) {
        in->setInput(settings()->server.tmsIp);
    }
}

STATE_DEF_ENTER(EnterPort) {
    Input * in = STATE_INPUT;
    snprintf(ip, sizeof(ip), "%s", in->input);
    GOTO_INPUT(state->parent, enterServerId, phraseGetDef(PHRASE_ENTER_SERV_PORT),
        "", 4, IN_MODE_NUMBERS, NULL);
    char str[5];
    if (serverItem == SERV_SET_MAIN) {
        intToStr(settings()->server.mainServerPort, str, sizeof(str));
    } else if (serverItem == SERV_SET_TMS) {
        intToStr(settings()->server.tmsPort, str, sizeof(str));
    }
    in->setInput(str);
}

STATE_DEF_ENTER(EnterServerId) {
    Input * in = STATE_INPUT;
    port = toInt(in->input);
    GOTO_INPUT(state->parent, getServerId, phraseGetDef(PHRASE_ENTER_SERV_ID),
        "", 4, IN_MODE_NUMBERS, NULL);
    char str[5];
    if (serverItem == SERV_SET_MAIN) {
        intToStr(settings()->server.mainServerId, str, sizeof(str));
    } else if (serverItem == SERV_SET_TMS) {
        intToStr(settings()->server.tmsId, str, sizeof(str));
    }
    in->setInput(str);
}

STATE_DEF_ENTER(GetServerId) {
    Input * in = STATE_INPUT;
    serverId = toInt(in->input);
    SM_GOTO(success);
}

static void enSSL() {
    settings()->server.sslEn = 1;
}

static void disSSL() {
    settings()->server.sslEn = 0;
}

STATE_DEF_ENTER(EnableSsl) {
    uiMenu(&sslMenu, getDisplay()->screen);
    sslMenu.checkEnable = true;
    OOP_CALL(&sslMenu, addItem, phraseGetDef(PHRASE_ENABLE), success, enSSL, NULL);
    OOP_CALL(&sslMenu, addItem, phraseGetDef(PHRASE_DISABLE), success, disSSL, NULL);
    GOTO_MENU(state->parent, &sslMenu, NULL, NULL);
    OOP_CALL(&sslMenu, setChecked, !settings()->server.sslEn);
}

STATE_DEF_ENTER(Success) {
    if (serverItem == SERV_SET_MAIN) {
        snprintf(settings()->server.mainServerIp,
                    sizeof(settings()->server.mainServerIp), "%s", ip);
        settings()->server.mainServerPort = port;
        settings()->server.mainServerId = serverId;
    } else if (serverItem == SERV_SET_TMS) {
        snprintf(settings()->server.tmsIp,
                    sizeof(settings()->server.tmsIp), "%s", ip);
        settings()->server.tmsPort = port;
        settings()->server.tmsId = serverId;
    }
    settings()->save();
    GOTO_INFO(state->parent, state->parent, phraseGetDef(PHRASE_SUC_DONME), "");
}

static void setItemToMainServer() {
    serverItem = SERV_SET_MAIN;
}

static void setItemToTms() {
    serverItem = SERV_SET_TMS;
}

STATE_DEF_ENTER(NetworkSettings) {
    uiMenu(&serverSetMenu, getDisplay()->screen);
    OOP_CALL(&serverSetMenu, addItem, serverSetTxt[SERV_SET_MAIN], enterIp, setItemToMainServer, NULL);
    OOP_CALL(&serverSetMenu, addItem, serverSetTxt[SERV_SET_TMS], enterIp, setItemToTms, NULL);
    OOP_CALL(&serverSetMenu, addItem, serverSetTxt[SERV_SET_SSL], enableSsl, NULL, NULL);
    GOTO_MENU(state->parent, &serverSetMenu, NULL, NULL);
}

OOP_CTOR(NetworkSettings, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(NetworkSettings);

    enterIp = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterIp, self, "enter ip");
    enterIp->vtable.enter = STATE_ENTER(EnterIp);

    enterPort = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterPort, self, "enter port");
    enterPort->vtable.enter = STATE_ENTER(EnterPort);

    enableSsl = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enableSsl, self, "enable ssl");
    enableSsl->vtable.enter = STATE_ENTER(EnableSsl);

    enterServerId = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, enterServerId, self, "enter server id");
    enterServerId->vtable.enter = STATE_ENTER(EnterServerId);

    success = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, success, self, "sucess server settings");
    success->vtable.enter = STATE_ENTER(Success);

    getServerId = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, getServerId, self, "get server id");
    getServerId->vtable.enter = STATE_ENTER(GetServerId);
}

/******************** Configuration sub state **********************/

STATE_DEF_ENTER(Configuration) {
    GOTO_DEV_INFO(state->parent);
}

OOP_CTOR(Configuration, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Configuration);
}

/******************** Merchant pass reset sub state **********************/

STATE_DEF_ENTER(MerchantPassReset) {
    snprintf(settings()->terminal.merchantPin,
                MERCHANT_PIN_LEN + 1, "%s", MERCHANT_DEFAULT_PIN);
    settings()->save();
    GOTO_INFO(state->parent, state->parent, phraseGetDef(PHRASE_SUC_DONME), "");
}

OOP_CTOR(MerchantPassReset, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(MerchantPassReset);
}

/******************** FARA sub state **********************/

STATE_DEF_ENTER(FARA) {
    GOTO_DEV_INFO(state->parent);
}

OOP_CTOR(FARA, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(FARA);
}

/******************** Update app sub state **********************/

STATE_DEF_ENTER(UpdateApp) {
    GOTO_DEV_INFO(state->parent);
}

OOP_CTOR(UpdateApp, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(UpdateApp);
}

/******************** Default Settings sub state **********************/

STATE_DEF_ENTER(DefaultSettings) {
    settings()->reset();
    GOTO_INFO(state->parent, state->parent, phraseGetDef(PHRASE_SUC_DONME), "");
}

OOP_CTOR(DefaultSettings, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(DefaultSettings);
}

/******************** Supervisor menu sub state **********************/

static Menu menu;

STATE_DEF_ENTER(SupervisorMenu) {
    uiMenu(&menu, getDisplay()->screen);
    for (uint8_t i = 0; i < SUBS_ALL ; i++) {
        OOP_CALL(&menu, addItem, phraseGetDef(itemTxt[i]), subStates[i], NULL, NULL);
    }
    GOTO_MENU(getState(STATE_ID_SUPPORTER), &menu, NULL, NULL);
}

static void SupervisorMenu(State *parent) {
    supervisorMenu = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, supervisorMenu, parent, "supervisor menu");
    supervisorMenu->vtable.enter = STATE_ENTER(SupervisorMenu);
}

/******************************************************************/

STATE_DEF_ENTER(Supervisor) {
    SM_GOTO(enterPass);
}

STATE_DEF_EXIT(Supervisor) {
}

OOP_CTOR(Supervisor, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Supervisor);
    self->base.vtable.exit = STATE_EXIT(Supervisor);

    EnterPassword(self);
    SupervisorMenu(self);
    ChangeMerPin(self);

    subStates[SUBS_NET_SETTINGS] = (NetworkSettings *)GET_MEM(sizeof(NetworkSettings));
    OOP_CALL_CTOR(NetworkSettings, subStates[SUBS_NET_SETTINGS], supervisorMenu, "terminal settings");
    subStates[SUBS_KEY_INJECTION] = (KeyInjection *)GET_MEM(sizeof(KeyInjection));
    OOP_CALL_CTOR(KeyInjection, subStates[SUBS_KEY_INJECTION], supervisorMenu, "key injection");
    subStates[SUBS_CONFIGURATION] = (Configuration *)GET_MEM(sizeof(Configuration));
    OOP_CALL_CTOR(Configuration, subStates[SUBS_CONFIGURATION], supervisorMenu, "configuration");
    subStates[SUBS_FARA] = (FARA *)GET_MEM(sizeof(FARA));
    OOP_CALL_CTOR(FARA, subStates[SUBS_FARA], supervisorMenu, "FARA");
    subStates[SUBS_MERCHANT_PASS_RESET] = (MerchantPassReset *)GET_MEM(sizeof(MerchantPassReset));
    OOP_CALL_CTOR(MerchantPassReset, subStates[SUBS_MERCHANT_PASS_RESET], supervisorMenu, "merchant pass reset");
    subStates[SUBS_UPDATE_APP] = (UpdateApp *)GET_MEM(sizeof(UpdateApp));
    OOP_CALL_CTOR(UpdateApp, subStates[SUBS_UPDATE_APP], supervisorMenu, "update app");
    subStates[SUBS_DEFAULT_SETTINGS] = (DefaultSettings *)GET_MEM(sizeof(DefaultSettings));
    OOP_CALL_CTOR(DefaultSettings, subStates[SUBS_DEFAULT_SETTINGS], supervisorMenu, "default settings");

}