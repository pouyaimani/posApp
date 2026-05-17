#include "states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "services/services.h"
#include "ui/menu.h"
#include "sys/sys.h"
#include "merchant/merchant.h"
#include "supervisor/supervisor.h"
#include "phrases/phrases.h"

static Menu menu;
static SubState *powerOff;
static Merchant *merchant;
static Merchant *supervisor;

#define ITEM_CNT_MAX    4

static void onCustomer() {
    CardHolder *ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = false;
    SM_GOTO(getState(STATE_ID_CARD_HOLDER));
}

static void onExit() {
    GOTO_DIAL(STATE_SUPPORTER, powerOff, phraseGetDef(PHRASE_WANNA_EXIT), "");
}

STATE_DEF_ENTER(Supporter) {
    ui_menu_create(&menu, disp()->screen);
    OOP_CALL(&menu, addItem, phraseGetDef(PHRASE_CUSTOMER), NULL, onCustomer, NULL);
    OOP_CALL(&menu, addItem, phraseGetDef(PHRASE_MERCHANT), merchant, NULL, NULL);
    OOP_CALL(&menu, addItem, phraseGetDef(PHRASE_SUPERVISOR), supervisor, NULL, NULL);
    OOP_CALL(&menu, addItem, phraseGetDef(PHRASE_TURN_OFF), NULL, onExit, NULL);
    GOTO_MENU(STATE_IDLE, &menu, NULL, NULL);
}

STATE_DEF_HANDLE(Supporter, TimeOutEvent) {

}

/******************** Power off sub state **********************/

STATE_DEF_ENTER(PowerOff) {
    GOTO_INFO(NULL, NULL, phraseGetDef(PHRASE_POWERING_OFF), "");
    OOP_CALL(sys(), powerOff);
}

OOP_CTOR(Supporter, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Supporter);
    self->base.vtable.handleTimeout = STATE_HANDLE(Supporter, TimeOutEvent);

    powerOff = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, powerOff, self, "power off");
    powerOff->vtable.enter = STATE_ENTER(PowerOff);

    merchant = (Merchant*)MEM_ALLOC(sizeof(Merchant));
    OOP_CALL_CTOR(Merchant, merchant, self, "merchant");

    supervisor = (Supervisor*)MEM_ALLOC(sizeof(Supervisor));
    OOP_CALL_CTOR(Supervisor, supervisor, self, "supervisor");
}