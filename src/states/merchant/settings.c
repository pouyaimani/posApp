#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/menu.h"
#include "ui/bar.h"
#include "sys/sys.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"

typedef enum {
    SET_ITEM_SOUND = 0,
    SET_ITEM_ENERGY,
    SET_ITEM_RECEIPT,
    SET_ITEM_SCR_LIGHT,
    SET_ITEM_TOUCH,
    SET_ITEM_DATE_TIME,
    SET_ITEM_ALL
} SettingsItem_t;

static SubState *subSettings[SET_ITEM_ALL];

static const Phrases_t SettingsItemTxt[SET_ITEM_ALL] = {
    PHRASE_SOUND_SETTINGS,
    PHRASE_ENERGY_SAVING,
    PHRASE_RECEIPT_SETTINGS,
    PHRASE_SCR_BRIGHTNESS,
    PHRASE_TOUCH_STATUS,
    PHRASE_DT_UPDATING,
};

/******************** sound sub state **********************/

static Bar soundBar;

STATE_DEF_ENTER(SoundSettings) {
    ui_bar_create(&soundBar, disp()->screen, 0, sys()->maxSound);
    ui_bar_set_title(&soundBar, "تنظیم صدا");
    ui_bar_set_value(&soundBar, settings()->terminal.devVolume);
    ui_bar_show(&soundBar);
}

STATE_DEF_EXIT(SoundSettings) {
    settings()->terminal.devVolume = soundBar.value;
    settings()->save();
    ui_bar_hide(&soundBar);
    ui_bar_destroy(&soundBar);
}

STATE_DEF_HANDLE(SoundSettings, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {

    } else if (ev->key == KEY_UP) {
        ui_bar_inc(&soundBar);
    } else if (ev->key == KEY_DOWN) {
        ui_bar_dec(&soundBar);
    }
    OOP_CALL(sys(), setVolume, soundBar.value);
}

static void SoundSettings(State *parent) {
    subSettings[SET_ITEM_SOUND] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_SOUND], parent, "sound settings");
    subSettings[SET_ITEM_SOUND]->vtable.enter = STATE_ENTER(SoundSettings);
    subSettings[SET_ITEM_SOUND]->vtable.exit = STATE_EXIT(SoundSettings);
    subSettings[SET_ITEM_SOUND]->vtable.handleKeypad = STATE_HANDLE(SoundSettings, KeypadEvent);
}

/******************** energy sub state **********************/
static const char* energyItemTxt[2] = {
    "بازه ذخیره انرژی",
    "بازه خاموشی"
};

static Menu *energyMenu;
static int idx;
static SubState *getValue;

STATE_DEF_ENTER(EnergySettings) {
    ui_menu_create(energyMenu, disp()->screen);
    for (uint8_t i = 0; i < 2 ; i++) {
        ui_menu_addItem(energyMenu, &energyItemTxt[i], NULL, NULL, NULL);
    }
    ui_menu_show(energyMenu);
}

STATE_DEF_EXIT(EnergySettings) {
    ui_menu_hide(energyMenu);
    ui_menu_destroy(energyMenu);
}

STATE_DEF_HANDLE(EnergySettings, KeypadEvent) {
    ui_menu_handleItem(energyMenu, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        idx = energyMenu->idx;
        if (energyMenu->idx == 0) {
            GOTO_INPUT(state, getValue, "ورود بازه ذخیره انرژی", "", 2, IN_MODE_NUMBERS, NULL);
        } else if (energyMenu->idx == 1) {
            GOTO_INPUT(state, getValue, "ورود بازه خاموشی", "", 2, IN_MODE_NUMBERS, NULL);
        }
    }
}

STATE_DEF_ENTER(GetValue) {
    Input *in = (Input*)getState(STATE_ID_INPUT);
    in->input;
    if(idx == 0) {

    } else if (idx == 2) {

    }
    GOTO_INFO(state->parent, state->parent, "با موفقیت انجام شد", "");
}

STATE_DEF_EXIT(GetValue) {
    
}

static void EnergySettings(State *parent) {
    subSettings[SET_ITEM_ENERGY] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_ENERGY], parent, "energy settings");
    subSettings[SET_ITEM_ENERGY]->vtable.enter = STATE_ENTER(EnergySettings);
    subSettings[SET_ITEM_ENERGY]->vtable.exit = STATE_EXIT(EnergySettings);
    subSettings[SET_ITEM_ENERGY]->vtable.handleKeypad = STATE_HANDLE(EnergySettings, KeypadEvent);

    getValue = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, getValue, subSettings[SET_ITEM_ENERGY], "get value");
    getValue->vtable.enter = STATE_ENTER(GetValue);
    getValue->vtable.exit = STATE_EXIT(GetValue);
}

/******************** receipt sub state **********************/
static const char* receiptItemTxt[4] = {
    "چاپ خودکار رسید",
    "زمان رسید دوم",
    "چاپ رسید پذیرنده",
    "مدل چاپ",
};

static SubState *subReceipt[4];

static Menu *receiptMenu;

STATE_DEF_ENTER(ReceiptSettings) {
    ui_menu_create(receiptMenu, disp()->screen);
    for (uint8_t i = 0; i < 4 ; i++) {
        ui_menu_addItem(receiptMenu, &receiptItemTxt[i], subReceipt[i], NULL, NULL);
    }
    GOTO_MENU(state->parent, receiptMenu, NULL, NULL);
}

static Menu *autoRecMenu;

STATE_DEF_ENTER(AutoPrint) {
    ui_menu_on_off(autoRecMenu, disp()->screen);
    ui_menu_show(autoRecMenu);
}

STATE_DEF_EXIT(AutoPrint) {
    ui_menu_hide(autoRecMenu);
    ui_menu_destroy(autoRecMenu);
}

STATE_DEF_HANDLE(AutoPrint, KeypadEvent) {
    ui_menu_handleItem(autoRecMenu, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        ui_menu_set_checked(autoRecMenu, autoRecMenu->idx);
    }
}

static SubState *secPrintSuc;

STATE_DEF_ENTER(SecPrintTime) {
    GOTO_INPUT(state->parent, secPrintSuc, "زمان رسید دوم", "", 2, IN_MODE_NUMBERS, NULL);
}

STATE_DEF_ENTER(SecPrintTimeSuc) {
    GOTO_INFO(state->parent, state->parent, "با موفقیت انجام شد", "");
}

static Menu merchRecMenu;

STATE_DEF_ENTER(PrnMerchRec) {
    ui_menu_on_off(&merchRecMenu, disp()->screen);
    ui_menu_show(&merchRecMenu);
}

STATE_DEF_EXIT(PrnMerchRec) {
    ui_menu_hide(&merchRecMenu);
    ui_menu_destroy(&merchRecMenu);
}

STATE_DEF_HANDLE(PrnMerchRec, KeypadEvent) {
    ui_menu_handleItem(&merchRecMenu, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        ui_menu_set_checked(&merchRecMenu, merchRecMenu.idx);
    }
}

static Menu *prnModel;

STATE_DEF_ENTER(PrnModel) {
    ui_menu_create(prnModel, disp()->screen);
    ui_menu_addItem(prnModel, "پس زمینه سفید", NULL, NULL, NULL);
    ui_menu_addItem(prnModel, "پس زمینه مشکی", NULL, NULL, NULL);
    ui_menu_show(prnModel);
}

STATE_DEF_EXIT(PrnModel) {
    ui_menu_hide(prnModel);
    ui_menu_destroy(prnModel);
}

STATE_DEF_HANDLE(PrnModel, KeypadEvent) {
    ui_menu_handleItem(prnModel, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        ui_menu_set_checked(prnModel, prnModel->idx);
    }
}

static void ReceiptSettings(State *parent) {
    subSettings[SET_ITEM_RECEIPT] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_RECEIPT], parent, "receipt settings");
    subSettings[SET_ITEM_RECEIPT]->vtable.enter = STATE_ENTER(ReceiptSettings);

    subReceipt[0] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[0], subSettings[SET_ITEM_RECEIPT], "auto print");
    subReceipt[0]->vtable.enter = STATE_ENTER(AutoPrint);
    subReceipt[0]->vtable.exit = STATE_EXIT(AutoPrint);
    subReceipt[0]->vtable.handleKeypad = STATE_HANDLE(AutoPrint, KeypadEvent);

    subReceipt[1] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[1], subSettings[SET_ITEM_RECEIPT], "sec print time");
    subReceipt[1]->vtable.enter = STATE_ENTER(SecPrintTime);

    secPrintSuc = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, secPrintSuc, subSettings[SET_ITEM_RECEIPT], "sec print time suc");
    secPrintSuc->vtable.enter = STATE_ENTER(SecPrintTimeSuc);

    subReceipt[2] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[2], subSettings[SET_ITEM_RECEIPT], "print merchaant receipt");
    subReceipt[2]->vtable.enter = STATE_ENTER(PrnMerchRec);
    subReceipt[2]->vtable.exit = STATE_EXIT(PrnMerchRec);
    subReceipt[2]->vtable.handleKeypad = STATE_HANDLE(PrnMerchRec, KeypadEvent);

    subReceipt[3] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[3], subSettings[SET_ITEM_RECEIPT], "print model");
    subReceipt[3]->vtable.enter = STATE_ENTER(PrnModel);
    subReceipt[3]->vtable.exit = STATE_EXIT(PrnModel);
    subReceipt[3]->vtable.handleKeypad = STATE_HANDLE(PrnModel, KeypadEvent);
}

/******************** screen light sub state **********************/
static Bar brightBar;

STATE_DEF_ENTER(ScrLightSettings) {
    ui_bar_create(&brightBar, disp()->screen, 1, sys()->maxBright);
    ui_bar_set_title(&brightBar, "تنظیم نور صفحه");
    ui_bar_set_value(&brightBar, settings()->terminal.brightness);
    ui_bar_show(&brightBar);
}

STATE_DEF_EXIT(ScrLightSettings) {
    settings()->terminal.brightness = brightBar.value;
    settings()->save();
    ui_bar_hide(&brightBar);
    ui_bar_destroy(&brightBar);
}

STATE_DEF_HANDLE(ScrLightSettings, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {

    } else if (ev->key == KEY_UP) {
        ui_bar_inc(&brightBar);
    } else if (ev->key == KEY_DOWN) {
        ui_bar_dec(&brightBar);
    }
    OOP_CALL(sys(), setBrightness, brightBar.value);
}

static void ScrLightSettings(State *parent) {
    subSettings[SET_ITEM_SCR_LIGHT] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_SCR_LIGHT], parent, "receipt settings");
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.enter = STATE_ENTER(ScrLightSettings);
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.exit = STATE_EXIT(ScrLightSettings);
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.handleKeypad = STATE_HANDLE(ScrLightSettings, KeypadEvent);
}

/******************** touch sub state **********************/

static Menu *touchMenu;

STATE_DEF_ENTER(TouchSettings) {
    ui_menu_on_off(touchMenu, disp()->screen);
    int idx = settings()->terminal.touchEnable == true ? 0 : 1;
    ui_menu_set_checked(touchMenu, idx);
    ui_menu_show(touchMenu);
}

STATE_DEF_EXIT(TouchSettings) {
    ui_menu_hide(touchMenu);
    ui_menu_destroy(touchMenu);
}

STATE_DEF_HANDLE(TouchSettings, KeypadEvent) {
    ui_menu_handleItem(touchMenu, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        bool en = touchMenu->idx == 0;
        //TODO: enable/ disable touch
        settings()->terminal.touchEnable = en;
        ui_menu_set_checked(touchMenu, touchMenu->idx);
    }
}

static void TouchSettings(State *parent) {
    subSettings[SET_ITEM_TOUCH] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_TOUCH], parent, "touch settings");
    subSettings[SET_ITEM_TOUCH]->vtable.enter = STATE_ENTER(TouchSettings);
    subSettings[SET_ITEM_TOUCH]->vtable.exit = STATE_EXIT(TouchSettings);
    subSettings[SET_ITEM_TOUCH]->vtable.handleKeypad = STATE_HANDLE(TouchSettings, KeypadEvent);
}

/******************** date time sub state **********************/

STATE_DEF_ENTER(DateTimeSettings) {
    SHOW_INFO("لطفا منتظر بمانید", "");
}

STATE_DEF_EXIT(DateTimeSettings) {
}

STATE_DEF_HANDLE(DateTimeSettings, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        GOTO_INFO(state->parent, state->parent, "خطا در به روز رسانی", "");
    }
}

static void DateTimeSettings(State *parent) {
    subSettings[SET_ITEM_DATE_TIME] = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_DATE_TIME], parent, "date time settings");
    subSettings[SET_ITEM_DATE_TIME]->vtable.enter = STATE_ENTER(DateTimeSettings);
    subSettings[SET_ITEM_DATE_TIME]->vtable.exit = STATE_EXIT(DateTimeSettings);
    subSettings[SET_ITEM_DATE_TIME]->vtable.handleKeypad = STATE_HANDLE(DateTimeSettings, KeypadEvent);
}

/******************** Settings sub state **********************/
static Menu *settingsMenu;

static void createUi() {
    ui_menu_create(settingsMenu, disp()->screen);
    for (uint8_t i = 0; i < SET_ITEM_ALL ; i++) {
        ui_menu_addItem(settingsMenu, phraseGetDef(SettingsItemTxt[i]), subSettings[i], NULL, NULL);
    }
}

STATE_DEF_ENTER(Settings) {
    createUi();
    GOTO_MENU(state->parent, settingsMenu, NULL, NULL);
}

OOP_CTOR(Settings, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Settings);
    SoundSettings(self);
    EnergySettings(self);
    ReceiptSettings(self);
    ScrLightSettings(self);
    TouchSettings(self);
    DateTimeSettings(self);

    energyMenu = MEM_ALLOC(sizeof(*energyMenu));
    receiptMenu = MEM_ALLOC(sizeof(*receiptMenu));
    prnModel = MEM_ALLOC(sizeof(*prnModel));

    touchMenu = MEM_ALLOC(sizeof(*touchMenu));

    autoRecMenu = MEM_ALLOC(sizeof(*autoRecMenu));
    
}