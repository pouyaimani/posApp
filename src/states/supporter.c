#include "supporter.h"
#include "states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "transactions/transaction.h"
#include "ui/menu.h"
#include "sys/sys.h"
#include "merchant/merchant.h"
#include "supervisor/supervisor.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "menuMgr/menuMgr.h"
#include "menuMgr/menuMgr.h"

static Menu*       menu;
static Merchant*   merchant;
static Supervisor* supervisor;

#define ITEM_CNT_MAX 4

static void onCustomer() {
    CardHolder* ch  = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = false;
    SM_GOTO(getState(STATE_ID_CARD_HOLDER));
}

static poweroff(void* arg) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_POWERING_OFF), "");
    OOP_CALL(infoPage(), forceUpdate);
    OOP_CALL(sys(), powerOff);
}

static void onExit() {
    GOTO_DIAL(STATE_SUPPORTER, STATE_SUPPORTER, phraseGetDef(PHRASE_WANNA_EXIT),
              "", poweroff, NULL);
}

STATE_DEF_ENTER(Supporter) {
    ui_menu_create(menu, disp()->screen);
    ui_menu_addItem(menu, phraseGetDef(PHRASE_CUSTOMER), LV_TEXT_ALIGN_RIGHT,
                    NULL, onCustomer, NULL);
    ui_menu_addItem(menu, phraseGetDef(PHRASE_MERCHANT), LV_TEXT_ALIGN_RIGHT,
                    merchant, NULL, NULL);
    ui_menu_addItem(menu, phraseGetDef(PHRASE_SUPERVISOR), LV_TEXT_ALIGN_RIGHT,
                    supervisor, NULL, NULL);
    ui_menu_addItem(menu, phraseGetDef(PHRASE_TURN_OFF), LV_TEXT_ALIGN_RIGHT,
                    NULL, onExit, NULL);
    menuMgr()->run(menu, STATE_IDLE, &menuCfgBack2idleOBC);
}

STATE_DEF_HANDLE(Supporter, TimeOutEvent) { SM_GOTO(STATE_IDLE); }

/******************** Power off sub state **********************/

OOP_CTOR(Supporter, State* parent, const char* name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(Supporter);
    self->base.vtable.handleTimeout = STATE_HANDLE(Supporter, TimeOutEvent);

    merchant = (Merchant*)MEM_ALLOC(sizeof(Merchant));
    OOP_CALL_CTOR(Merchant, merchant, self, "merchant");

    supervisor = (Supervisor*)MEM_ALLOC(sizeof(Supervisor));
    OOP_CALL_CTOR(Supervisor, supervisor, self, "supervisor");

    self->substate.customer   = NULL;
    self->substate.merchant   = merchant;
    self->substate.supervisor = supervisor;

    menu = MEM_ALLOC(sizeof(*menu));
}