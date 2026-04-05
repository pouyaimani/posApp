#include "states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "services/services.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "merchant/merchant.h"
#include "supervisor/supervisor.h"

static Menu menu;
static SubState *powerOff;
static Merchant *merchant;
static Merchant *supervisor;

#define ITEM_CNT_MAX    4

static const char* itemTxt[ITEM_CNT_MAX] = {
    "مشتری",
    "پذیرنده",
    "سرپرست",
    "خاموش کردن",
};

static void onCustomer() {
    CardHolder *ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = false;
    SM_GOTO(getState(STATE_ID_CARD_HOLDER));
}

static void onExit() {
    GOTO_DIAL(STATE_SUPPORTER, powerOff, "قصد خروج دارید؟", "");
}

STATE_DEF_ENTER(Supporter) {
    uiMenu(&menu, getDisplay()->screen);
    OOP_CALL(&menu, addItem, itemTxt[0], NULL, onCustomer, NULL);
    OOP_CALL(&menu, addItem, itemTxt[1], merchant, NULL, NULL);
    OOP_CALL(&menu, addItem, itemTxt[2], supervisor, NULL, NULL);
    OOP_CALL(&menu, addItem, itemTxt[3], NULL, onExit, NULL);
    GOTO_MENU(STATE_IDLE, &menu, NULL, NULL);
}

STATE_DEF_HANDLE(Supporter, TimeOutEvent) {

}

/******************** Power off sub state **********************/

STATE_DEF_ENTER(PowerOff) {
    GOTO_INFO(NULL, NULL, "در حال خاموش شدن ...", "");
    OOP_CALL(getDevice(), powerOff);
}

OOP_CTOR(Supporter, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Supporter);
    self->base.vtable.handleTimeout = STATE_HANDLE(Supporter, TimeOutEvent);

    powerOff = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, powerOff, self, "power off");
    powerOff->vtable.enter = STATE_ENTER(PowerOff);

    merchant = (Merchant*)GET_MEM(sizeof(Merchant));
    OOP_CALL_CTOR(Merchant, merchant, self, "merchant");

    supervisor = (Supervisor*)GET_MEM(sizeof(Supervisor));
    OOP_CALL_CTOR(Supervisor, supervisor, self, "supervisor");
}