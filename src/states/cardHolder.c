
#include "cardHolder.h"
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
#include "txn.h"
#include "error.h"
#include "settings/settings.h"

size_t                  LEN_CARDHOLDER_ITEMS;
static Menu*            menu;
static Menu             chargeMenu;
static CardHolderItems* activeItems[LEN_MAX_CARDHOLDER_ITEMS];
static CardHolderItems* selected;

static void onPurchase() { SM_GOTO(&getTxn(TXN_PURCHASE)->state); }

static void onBalance() { SM_GOTO(&getTxn(TXN_BALANCE)->state); }

static void onBill() { SM_GOTO(&getTxn(TXN_BILL)->state); }

static void onSimCharge() {
    ui_menu_create(&chargeMenu, disp()->screen);
    ui_menu_addItem(&chargeMenu, phraseGetDef(PHRASE_TXN_TOPUP),
                    LV_TEXT_ALIGN_RIGHT, &getTxn(TXN_TOPUP)->state, NULL, NULL);
    ui_menu_addItem(&chargeMenu, phraseGetDef(PHRASE_TXN_VOUCHER),
                    LV_TEXT_ALIGN_RIGHT, &getTxn(TXN_VOUCHER)->state, NULL,
                    NULL);
    GOTO_MENU(STATE_IDLE, &chargeMenu, NULL, NULL);
}

static CardHolderItems __cardHolderItems[] = {
    {
        .name    = "خرید",
        .enabled = false,
        .handler = onPurchase,
        .usage   = 0,
    },
    {
        .name    = "موجودی",
        .enabled = false,
        .handler = onBalance,
        .usage   = 0,
    },
    {
        .name    = "پرداخت قبض",
        .enabled = false,
        .handler = onBill,
        .usage   = 0,
    },
    {
        .name    = "خرید شارژ",
        .enabled = false,
        .handler = onSimCharge,
        .usage   = 0,
    },
};

CardHolderItems* cardHolderItems(int i) {
    RETURN_VALUE_IF_GE(i, LEN_CARDHOLDER_ITEMS, ;, NULL);
    return &__cardHolderItems[i];
}

static void createUi() {
    ui_menu_create(menu, disp()->screen);
    int cnt = 0;
    for (uint8_t i = 0; i < LEN_CARDHOLDER_ITEMS; i++) {
        if (cardHolderItems(i)->enabled) {
            ui_menu_addItem(menu, cardHolderItems(i)->name, LV_TEXT_ALIGN_RIGHT,
                            NULL, NULL, NULL);
            activeItems[cnt++] = cardHolderItems(i);
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

static void startItem() {
    statusBar()->setInfo(selected->name);
    if (selected->handler) {
        selected->handler();
    }
    selected->usage++;
}

STATE_DEF_HANDLE(CardHolder, MagEvent) {
    CardHolder* ch = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    if (!ch->isMagSwiped) {
        startItem();
        getEventloop()->unregisterChecker(magreader()->ioRead);
    }
}

STATE_DEF_HANDLE(CardHolder, KeypadEvent) {
    ui_menu_handleItem(menu, ev->key);
    CardHolder* ch = (CardHolder*)state;
    if (ev->key <= KEY_9) {
        selected = activeItems[((int)ev->key - 1)];
        if (!ch->isMagSwiped) {
            OOP_CALL(infoPage(), setData, INFO_IMG, ICON_SWIPE_CARD,
                     phraseGetDef(PHRASE_SWIPRE_CARD));
            ui_menu_hide(menu);
            OOP_CALL(infoPage(), show);
            return;
        }
        startItem();
    } else if (ev->key == KEY_ESC) {
        GOTO_IDLE();
    } else if (ev->key == KEY_ENTER) {
        selected = activeItems[menu->idx];
        if (!ch->isMagSwiped) {
            OOP_CALL(infoPage(), setData, INFO_IMG, ICON_SWIPE_CARD,
                     phraseGetDef(PHRASE_SWIPRE_CARD));
            ui_menu_hide(menu);
            OOP_CALL(infoPage(), show);
            return;
        }
        startItem();
    }
}

OOP_CTOR(CardHolder, State* parent, const char* name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(CardHolder);
    self->base.vtable.exit          = STATE_EXIT(CardHolder);
    self->base.vtable.handleKeypad  = STATE_HANDLE(CardHolder, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(CardHolder, TimeOutEvent);
    self->base.vtable.handleMag     = STATE_HANDLE(CardHolder, MagEvent);

    LEN_CARDHOLDER_ITEMS = sizeof(__cardHolderItems) / sizeof(CardHolderItems);

    LOG_DEBUG("LEN_CARDHOLDER_ITEMS = %d", LEN_CARDHOLDER_ITEMS);
    for (uint8_t i = 0; i < LEN_CARDHOLDER_ITEMS; i++) {
        __cardHolderItems[i].enabled = settings()->terminal.chItemStatus[i];
    }

    menu = MEM_ALLOC(sizeof(*menu));
}