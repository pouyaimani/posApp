#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "lvgl.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "services/services.h"
#include "ui/ui.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "statusBar/statusBar.h"

static Menu menu;
static InfoPage infop;
static ServiceId_t enableServicesId[SERVICE_ID_ALL];
static int selected;

static void createUi() {
    uiMenu(&menu, getDisplay()->screen);
    int cnt = 0;
    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        if (getService(i)->enable) {
            OOP_CALL(&menu, addItem, getService(i)->state.name, NULL, NULL, NULL);
            enableServicesId[cnt++] = i;
        }
    }
    OOP_CALL(&menu, hide);
}

STATE_DEF_ENTER(CardHolder) {
    CardHolder *ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    if (!ch->isMagSwiped) {
        getEventloop()->registerChecker(getMagReader()->ioRead);
    }
    createUi();
    OOP_CALL(&menu, show);
}

STATE_DEF_EXIT(CardHolder) {
    OOP_CALL(&menu, hide);
    OOP_CALL(&infop, hide);
    uiDeleteMenu(&menu);
}

STATE_DEF_HANDLE(CardHolder, TimeOutEvent) {

}

static void gotoService() {
    statusBar()->setInfo(getService(enableServicesId[selected])->state.name);
    SM_GOTO(&getService(enableServicesId[selected])->state);
}

STATE_DEF_HANDLE(CardHolder, MagEvent) {
    CardHolder *ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    if (!ch->isMagSwiped) {
        gotoService();
        getEventloop()->unregisterChecker(getMagReader()->ioRead);
    }
}

STATE_DEF_HANDLE(CardHolder, KeypadEvent) {
    OOP_CALL(&menu, handleItem, ev->key);
    CardHolder *ch = (CardHolder*)state;
    if (ev->key <= KEY_9) {
        selected = (ServiceId_t)((int)ev->key - 1);
        if (!ch->isMagSwiped) {
            OOP_CALL(&infop, setData, INFO_T_IMG, ICON_SWIPE_CARD, SWIPE_CARD_TEXT);
            OOP_CALL(&menu, hide);
            OOP_CALL(&infop, show);
            return;
        }
        gotoService();
    }  else if (ev->key == KEY_ESC) {
        GOTO_IDLE();
    } else if (ev->key == KEY_ENTER) {
        selected = (ServiceId_t)menu.idx;
        if (!ch->isMagSwiped) {
            OOP_CALL(&infop, setData, INFO_T_IMG, ICON_SWIPE_CARD, SWIPE_CARD_TEXT);
            OOP_CALL(&menu, hide);
            OOP_CALL(&infop, show);
            return;
        }
        gotoService();     
    }
}

OOP_CTOR(CardHolder, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(CardHolder);
    self->base.vtable.exit = STATE_EXIT(CardHolder);
    self->base.vtable.handleKeypad = STATE_HANDLE(CardHolder, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(CardHolder, TimeOutEvent);
    self->base.vtable.handleMag = STATE_HANDLE(CardHolder, MagEvent);
    infop = infoPage();
}