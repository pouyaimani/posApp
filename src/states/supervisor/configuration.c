#include "supervisor.h"
#include "common.h"
#include "states/states.h"
#include "phrases/phrases.h"
#include "display/display.h"
#include "sys/sys.h"
#include "utility/utility.h"
#include "network/network.h"
#include "settings/settings.h"
#include "ui/menu.h"
#include "ui/infoPage.h"
#include "txnFlow/txnFLow.h"
#include "txnCommon.h"
#include "transaction.h"
#include "menuMgr/menuMgr.h"

/******************** Configuration sub state **********************/

static Menu* cfgMenu;

STATE_DEF_ENTER(Configuration) {
    cfgMenu = (Menu*)MEM_ALLOC(sizeof(Menu));
    ui_menu_create(cfgMenu, disp()->screen);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_KEY), LV_TEXT_ALIGN_RIGHT,
                    &getTxn(TXN_LOGON)->state, NULL, NULL);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_MERCHANT_DATA),
                    LV_TEXT_ALIGN_RIGHT, &getTxn(TXN_CFG)->state, NULL, NULL);
    menuMgr()->run(cfgMenu, state->parent, &menuCfgBack2idleOBM);
}

OOP_CTOR(Configuration, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Configuration);
}