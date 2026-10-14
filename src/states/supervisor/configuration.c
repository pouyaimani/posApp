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
#include "menuMgr/menuMgr.h"
#include "transactions/txnMgr.h"
#include "transactions/txnConfig.h"

/******************** Configuration sub state **********************/

static Menu* cfgMenu;

static void transactionDone(TxnMgrResult result, TxnMgrStep step,
                            TxnMgrError err, int respCode) {

    if (result == TXN_MGR_RESULT_REJECT) {
        /* Display approval or print the transaction receipt. */
        return;
    }
    if (result == TXN_MGR_RESULT_SUCCESS) {
        /* Display approval or print the transaction receipt. */
        return;
    }

    if (err != TXN_MGR_ERR_NONE) {
        /*
         * Handle a local failure.
         * respCode may still contain approval if persistence failed
         * after receiving a successful host response.
         */
        return;
    }

    /* Handle the host decline in respCode. */
    (void)step;
}

static void onLogon() { txnMgr()->run(&logonTxnCfg, transactionDone); }

static void onConfig() { txnMgr()->run(&cfgTxnCfg, transactionDone); }

STATE_DEF_ENTER(Configuration) {
    cfgMenu = (Menu*)MEM_ALLOC(sizeof(Menu));
    ui_menu_create(cfgMenu, disp()->screen);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_KEY), LV_TEXT_ALIGN_RIGHT,
                    NULL, onLogon, NULL);
    ui_menu_addItem(cfgMenu, phraseGetDef(PHRASE_GET_MERCHANT_DATA),
                    LV_TEXT_ALIGN_RIGHT, NULL, onConfig, NULL);
    menuMgr()->run(cfgMenu, state->parent, &menuCfgBack2idleOBM);
}

OOP_CTOR(Configuration, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Configuration);
}