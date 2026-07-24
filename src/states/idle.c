#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "sys/sys.h"
#include "mylvgl.h"
#include "display.h"
#include "event.h"
#include "wifi/wifi.h"
#include "assets.h"
#include "font/myFont.h"
#include "statusBar/statusBar.h"
#include "storage/storage.h"
#include "network/network.h"
#include "timer.h"
#include "states/merchant/merchant.h"
#include "settings/settings.h"
#include "record/txnRecs.h"
#include "record/shiftRecs.h"
#include "utility/utility.h"
#include "receipt/receiptTemplates.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"
#include "file/file.h"
#include "ui/swipeHint.h"
#include "ui/digitalReceipt.h"
#include "assets.h"
#include "txnFlow/txnPendingMgr.h"

#define MENU_BAR_HEIGHT 46

static lv_obj_t* menuBar;
static lv_obj_t* swipCardText;
static lv_obj_t* swipCardCont;
static lv_obj_t* mainIcon;
static lv_obj_t* menuButton;
static lv_obj_t* menuIcon;
static lv_obj_t* menuText;
static lv_obj_t* merchantName;

static Timer*    timer;
static SwipeHint swipe;

static void wifiAutoConnect() {
    if (OOP_CALL(network(), getRoute) != NET_ROUTE_WIFI) {
        return;
    }
    WifiConnectSt_t conSt = OOP_CALL(wifi(), getConnectStatus);
    if (conSt == WIFI_CONNECT_SUCCEED || conSt == WIFI_CONNECT_UNDER_PROCESS) {
        return;
    }
    WifiApInfo_t apInfo  = {0};
    char         pwd[64] = {0};
    snprintf(apInfo.essid, sizeof(apInfo.essid), "%s",
             settings()->terminal.wfiSSID);
    snprintf(apInfo.mac, sizeof(apInfo.mac), "%s",
             settings()->terminal.wifiMac);
    apInfo.secMode = settings()->terminal.wifiEnc;
    snprintf(pwd, sizeof(pwd), "%s", settings()->terminal.wifiPwd);
    if (strlen(apInfo.essid) > 0 && strlen(pwd) > 0 && apInfo.secMode != 0) {
        OOP_CALL(wifi(), hconnect, &apInfo, pwd);
    }
}

static void timerCb() { wifiAutoConnect(); }

static void menuEventCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        SM_GOTO(getState(STATE_ID_SUPPORTER));
    }
}

static void setTextIfChanged(lv_obj_t* label, const char* txt) {
    if (strcmp(lv_label_get_text(label), txt) == 0) {
        return;
    }

    LV_SET_TEXT(label, txt);
}

STATE_DEF_ENTER(Idle) {
    CardHolder* ch  = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = false;
    getEventloop()->registerChecker(magreader()->ioRead);
    LOG_DEBUG("settings()->terminal.isCfgDone  = %d",
              settings()->terminal.isCfgDone);
    setTextIfChanged(merchantName,
                     settings()->terminal.isCfgDone
                         ? settings()->terminal.merchantName
                         : phraseGetDef(PHRASE_DEV_IS_NOT_CONFIGURED));
    LV_SHOW(menuBar);
    // LV_SHOW(swipCardCont);
    LV_SHOW(mainIcon);
    statusBar()->enDateTimeMode();
    shifts();
    swipeHintShow(&swipe);
}

STATE_DEF_EXIT(Idle) {
    getEventloop()->unregisterChecker(magreader()->ioRead);
    LV_HIDE(menuBar);
    // LV_HIDE(swipCardCont);
    LV_HIDE(mainIcon);
    swipeHintHide(&swipe);
}

STATE_DEF_HANDLE(Idle, TimeOutEvent) {}

#include "receipt/receipt.h"
#include "printer/printer.h"
TxnData     txn;
ReceiptData data;
static void print() {
    data.txn        = txn;
    data.type       = DOC_TXN;
    txn.core.amount = 1240000;
    snprintf(txn.core.pan, sizeof(txn.core.pan), "%s", "6037991622221111");
    txn.core.processCode = 12;
    txn.core.rrn         = 1399;
    txn.core.trace       = 6419;
    txn.core.stan        = 19000;
    txn.core.txnType     = TXN_SALE;
    txn.dateTime         = OOP_CALL(sys(), getPackedDateTime);
    Receipt rec;
    buildReceipt(&rec, &data);
    OOP_CALL(&rec, flush);
    OOP_CALL(&rec, destroy);
}

static uint32_t seed = 123456789;

static uint32_t my_rand() {
    seed = (1103515245 * seed + 12345);
    return seed;
}

static uint32_t my_rand_range(uint32_t max) { return my_rand() % max; }

static void rand_digits(char* buf, size_t len) {
    for (size_t i = 0; i < len; i++)
        buf[i] = '0' + my_rand_range(10);
    buf[len] = '\0';
}

static void rand_alnum(char* buf, size_t len) {
    const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (size_t i = 0; i < len; i++)
        buf[i] = chars[my_rand_range(36)];
    buf[len] = '\0';
}

static void rand_letters(char* buf, size_t len) {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (size_t i = 0; i < len; i++)
        buf[i] = chars[my_rand_range(26)];
    buf[len] = '\0';
}

void generate_random_txn(TxnData* t) {
    t->core.txnType = my_rand_range(255);

    rand_digits(t->core.processCode, 6);
    rand_digits(t->core.amount, 12);
    rand_digits(t->core.rrn, 6);
    rand_digits(t->core.trace, 6);
    uint32_t date, time;
    getDateTimeUint(&date, &time);
    LOG_DEBUG("date = %u, time = %u", date, time);
    t->dateTime = packDateTime(date, time);
    rand_digits(t->core.stan, 12);
    rand_digits(t->core.respCode, 2);
}

static void insertTxn() {
    TxnData txn;
    generate_random_txn(&txn);
    txnrecord()->insert(&txn);
}

static bool txnHand(const TxnData* txn, void* userData) {
    uint32_t date, time;
    unpackDateTime(txn->dateTime, &date, &time);
    LOG_DEBUG(
        "txn: date = %lu, time = %lu, trace = %s, refNum = %s, stan = %s, "
        "amount = %s",
        date, time, txn->core.trace, txn->core.rrn, txn->core.stan,
        txn->core.amount);
}

static void home_cb(lv_event_t* e) {
    LV_UNUSED(e);
    printf("Home clicked\n");
}

static void print_cb(lv_event_t* e) {
    LV_UNUSED(e);
    printf("Print clicked\n");
}

void show_receipt_page(void) {
    static ReceiptPage page;
    ReceiptHeader header = {.dateTitle = "تاریخ",
                            .date      = "1405/04/23",

                            .timeTitle = "ساعت",
                            .time      = "21:13:52",

                            .amount   = "17,210,816",
                            .currency = "ریال",

                            .statusTitle       = "انتقال موفق",
                            .statusDescription = "تراکنش با موفقیت انجام شد",

                            .statusIcon = ICON_SUCCEED};

    ReceiptDetails details = {
        .count = 3,

        .details = {{.icon       = ICON_TERMINAL,
                     .title      = "کد کارتخوان",
                     .value      = "17210864",
                     .valueColor = lv_palette_main(LV_PALETTE_RED)},

                    {.icon       = ICON_DOC1,
                     .title      = "مرجع",
                     .value      = "51724693300412121212121212121212",
                     .valueColor = lv_color_black()},

                    {.icon       = ICON_BANK,
                     .title      = "بانک",
                     .value      = "بانک سامان",
                     .valueColor = lv_palette_main(LV_PALETTE_RED)},

                    {.icon       = ICON_DOC2,
                     .title      = "پیگیری",
                     .value      = "621986-X-1315",
                     .valueColor = lv_color_black()}}};

    ReceiptButtons buttons = {.left = {.text     = "صفحه اصلی",
                                       .icon     = "",
                                       .callback = home_cb,
                                       .primary  = false},

                              .right = {.text     = "چاپ رسید",
                                        .icon     = "",
                                        .callback = print_cb,
                                        .primary  = true}};

    lv_obj_t* receipt = ui_receipt_create(&page, &header, &details, &buttons);
    LV_SHOW(receipt);
}
static int pendMgrDone() { SM_GOTO(STATE_IDLE); }

STATE_DEF_HANDLE(Idle, KeypadEvent) {
    if (ev->key == KEY_FUNCTION) {
        SM_GOTO(getState(STATE_ID_SUPPORTER));
    } else if (ev->key == KEY_CLEAR) {
        GOTO_DEV_INFO(state);
    } else if (ev->key == KEY_1) {
        print();
    } else if (ev->key == KEY_2) {
        DEFINE_BYTE_ARRAY(stan, 7);
        txnTraceInfo()->inc();
        prependZerosInt(txnTraceInfo()->stan, 6, stan, sizeof(stan));
        LOG_DEBUG("stan int= %u, stan string = %s", txnTraceInfo()->stan, stan);
    } else if (ev->key == KEY_3) {
        OOP_CALL(file(), remove, "/mtd0/txn_t_info");
    } else if (ev->key == KEY_4) {
        txnPendingMgr()->run(pendMgrDone);

    } else if (ev->key == KEY_5) {
        show_receipt_page();
    } else if (ev->key == KEY_6) {
        insertTxn();
    } else if (ev->key == KEY_7) {
        txnrecord()->iterate();
    } else if (ev->key == KEY_8) {
        txnrecord()->reset();
    } else if (ev->key == KEY_9) {
        txnrecord()->init();
    } else if (ev->key == KEY_0) {
        QueryOperator op;
        txnquery()->init(&op);
        uint64_t dt = 3683180498412;
        txnquery()->where(&op, TXN_REC_FIELD_TIMESTAMP, SELECT_EQ, &dt);
        txnrecord()->select(&op, txnHand, NULL);
    }
}

STATE_DEF_HANDLE(Idle, SocketConnectEvent) {
    if (ev->isConnected) {
        GOTO_INFO(STATE_IDLE, STATE_IDLE, INFO_ERROR, "error in connecting",
                  "");
    } else {
        GOTO_INFO(STATE_IDLE, STATE_IDLE, INFO_SUCCESS, "connected", "");
    }
    network()->disconnect();
}

STATE_DEF_HANDLE(Idle, MagEvent) {
    if (!settings()->terminal.isCfgDone) {
        GOTO_INFO(STATE_IDLE, STATE_IDLE, INFO_ERROR,
                  phraseGetDef(PHRASE_DEV_IS_NOT_CONFIGURED), "");
        return;
    }
    CardHolder* ch  = (CardHolder*)getState(STATE_ID_CARD_HOLDER);
    ch->isMagSwiped = true;
    if (settings()->terminal.fixedAmountItem == FIXED_AMNT_DIS) {
        SM_GOTO(getState(STATE_ID_CARD_HOLDER));
    } else {
        SM_GOTO(getState(STATE_ID_FIXED_AMOUNT));
    }
    LOG_TRACE("track1 = %s", ev->data->track1.data);
    LOG_TRACE("track2 = %s", ev->data->track2.data);
    LOG_TRACE("track3 = %s", ev->data->track3.data);
}

static void createUi() {
    menuBar = lv_obj_create(disp()->screen);
    LV_SET_SIZE(menuBar, DISP_HOR_RES + 20, MENU_BAR_HEIGHT + 20);
    LV_SET_BG_COLOR(menuBar, MAIN_THEME_COLOR);
    LV_ALIGN(menuBar, LV_ALIGN_BOTTOM_MID, 10, 20);
    LV_SET_RADIUS(menuBar, 20);
    LV_SCROLL_DISABLE(menuBar);
    lv_obj_set_scroll_dir(menuBar, LV_DIR_NONE);
    LV_CLICK_DISABLE(menuBar);

    merchantName = lv_label_create(menuBar);
    LV_SET_TEXT_FONT(merchantName, FONT_16);
    LV_SET_TEXT_COLOR(merchantName, COLOR_WHITE);
    LV_SET_TEXT(merchantName, settings()->terminal.isCfgDone
                                  ? settings()->terminal.merchantName
                                  : phraseGetDef(PHRASE_DEV_IS_NOT_CONFIGURED));
    LV_ALIGN(merchantName, LV_ALIGN_CENTER, -15, -10);

    mainIcon = lv_img_create(disp()->screen);
    lv_img_set_src(mainIcon, ICON_IDLE_MAIN);
    LV_ALIGN(mainIcon, LV_ALIGN_CENTER, 20, -30);
    LV_SCROLL_DISABLE(mainIcon);
    LV_CLICK_DISABLE(mainIcon);

    menuButton = lv_button_create(menuBar);
    LV_CLICKABLE(menuButton);
    LV_SET_BG_OPA(menuButton, LV_OPA_0);
    LV_SET_BORDER_OPA(menuButton, LV_OPA_0);
    LV_SET_SIZE(menuButton, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(menuButton, LV_ALIGN_TOP_RIGHT, -40, 5);
    LV_ADD_EV_CB(menuButton, menuEventCb, LV_EVENT_CLICKED, NULL);
    LV_SCROLL_DISABLE(menuButton);

    menuIcon = lv_img_create(menuButton);
    lv_img_set_src(menuIcon, ICON_MENU);
    LV_ALIGN(menuIcon, LV_ALIGN_TOP_MID, 0, 0);

    menuText = lv_label_create(menuButton);
    LV_SET_TEXT_FONT(menuText, FONT_16);
    LV_SET_TEXT_COLOR(menuText, COLOR_WHITE);
    LV_SET_TEXT(menuText, phraseGetDef(PHRASE_MENU));
    LV_ALIGN_TO(menuText, menuIcon, LV_ALIGN_OUT_BOTTOM_MID, 0, -5);

    swipeHintCreate(&swipe,
                    disp()->screen, // parent
                    SWIPE_DOWN,     // animation direction
                    7);             // number of arrows

    swipeHintAlign(&swipe, LV_ALIGN_RIGHT_MID, 20, -10);
}

OOP_CTOR(Idle, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter           = STATE_ENTER(Idle);
    self->base.vtable.exit            = STATE_EXIT(Idle);
    self->base.vtable.handleKeypad    = STATE_HANDLE(Idle, KeypadEvent);
    self->base.vtable.handleTimeout   = STATE_HANDLE(Idle, TimeOutEvent);
    self->base.vtable.handleMag       = STATE_HANDLE(Idle, MagEvent);
    self->base.vtable.onSocketConnect = STATE_HANDLE(Idle, SocketConnectEvent);

    createUi();

    timer = TIMER_CREATE(timerCb, SECS(10), false);
}