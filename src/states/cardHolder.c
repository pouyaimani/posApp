#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "lvgl.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "transactions/transaction.h"
#include "ui/menu.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "statusBar/statusBar.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"

static Menu*   menu;
static TxnId_t enableServicesId[TXN_ID_ALL];
static int     selected;

static void createUi() {
    ui_menu_create(menu, disp()->screen);
    int cnt = 0;
    for (uint8_t i = 0; i < TXN_ID_ALL; i++) {
        if (getTxn(i)->enable) {
            ui_menu_addItem(menu, getTxn(i)->state.name, LV_TEXT_ALIGN_RIGHT,
                            NULL, NULL, NULL);
            enableServicesId[cnt++] = i;
        }
    }
    ui_menu_hide(menu);
}

STATE_DEF_ENTER(CardHolder) {
    CardHolder* ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    if (!ch->isMagSwiped) {
        getEventloop()->registerChecker(magreader()->ioRead);
    }
    createUi();
    ui_menu_show(menu);
}

STATE_DEF_EXIT(CardHolder) {
    ui_menu_hide(menu);
    OOP_CALL(infoPage(), hide);
    ui_menu_destroy(menu);
}

STATE_DEF_HANDLE(CardHolder, TimeOutEvent) {}

static void gotoService() {
    statusBar()->setInfo(getTxn(enableServicesId[selected])->state.name);
    SM_GOTO(&getTxn(enableServicesId[selected])->state);
}

STATE_DEF_HANDLE(CardHolder, MagEvent) {
    CardHolder* ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    if (!ch->isMagSwiped) {
        gotoService();
        getEventloop()->unregisterChecker(magreader()->ioRead);
    }
}

STATE_DEF_HANDLE(CardHolder, KeypadEvent) {
    ui_menu_handleItem(menu, ev->key);
    CardHolder* ch = (CardHolder*)state;
    if (ev->key <= KEY_9) {
        selected = (TxnId_t)((int)ev->key - 1);
        if (!ch->isMagSwiped) {
            OOP_CALL(infoPage(), setData, INFO_IMG, ICON_SWIPE_CARD,
                     phraseGetDef(PHRASE_SWIPRE_CARD));
            ui_menu_hide(menu);
            OOP_CALL(infoPage(), show);
            return;
        }
        gotoService();
    } else if (ev->key == KEY_ESC) {
        GOTO_IDLE();
    } else if (ev->key == KEY_ENTER) {
        selected = (TxnId_t)menu->idx;
        if (!ch->isMagSwiped) {
            OOP_CALL(infoPage(), setData, INFO_IMG, ICON_SWIPE_CARD,
                     phraseGetDef(PHRASE_SWIPRE_CARD));
            ui_menu_hide(menu);
            OOP_CALL(infoPage(), show);
            return;
        }
        gotoService();
    }
}

OOP_CTOR(CardHolder, State* parent, const char* name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(CardHolder);
    self->base.vtable.exit          = STATE_EXIT(CardHolder);
    self->base.vtable.handleKeypad  = STATE_HANDLE(CardHolder, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(CardHolder, TimeOutEvent);
    self->base.vtable.handleMag     = STATE_HANDLE(CardHolder, MagEvent);

    menu = MEM_ALLOC(sizeof(*menu));
}