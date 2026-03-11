#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"
#include "storage/storage.h"

static Storage *storage;
static Device *dev;

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

static const char* SettingsItemTxt[SET_ITEM_ALL] = {
    "تنظیمات صدا",
    "ذخیره انرژی",
    "تنظیمات رسید",
    "نور صفحه",
    "وضعیت صفحه لمسی",
    "به روز رسانی زمان و تاریخ",
};

/******************** sound sub state **********************/

static Bar soundBar;

STATE_DEF_ENTER(SoundSettings) {
    uiBar(&soundBar, getDisplay()->screen, 0, dev->maxSound);
    OOP_CALL(&soundBar, setTitle, "تنظیم صدا");
    OOP_CALL(&soundBar, setValue, storage->settings->terminal.devVolume);
    OOP_CALL(&soundBar, show);
}

STATE_DEF_EXIT(SoundSettings) {
    storage->settings->terminal.devVolume = soundBar.value;
    SAVE_SETTINGS();
    OOP_CALL(&soundBar, hide);
    uiBarDelete(&soundBar);
}

STATE_DEF_HANDLE(SoundSettings, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {

    } else if (ev->key == KEY_UP) {
        OOP_CALL(&soundBar, increase);
    } else if (ev->key == KEY_DOWN) {
        OOP_CALL(&soundBar, decrease);
    }
    OOP_CALL(dev, setVolume, soundBar.value);
}

static void SoundSettings(State *parent) {
    subSettings[SET_ITEM_SOUND] = (SubState *)GET_MEM(sizeof(SubState));
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

static Menu energyMenu;
static int idx;
static SubState *getValue;

STATE_DEF_ENTER(EnergySettings) {
    uiMenu(&energyMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < 2 ; i++) {
        OOP_CALL(&energyMenu, addItem, &energyItemTxt[i], NULL, NULL, NULL);
    }
    OOP_CALL(&energyMenu, show);
}

STATE_DEF_EXIT(EnergySettings) {
    OOP_CALL(&energyMenu, hide);
    uiDeleteMenu(&energyMenu);
}

STATE_DEF_HANDLE(EnergySettings, KeypadEvent) {
    OOP_CALL(&energyMenu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        idx = energyMenu.idx;
        if (energyMenu.idx == 0) {
            GOTO_INPUT(state, getValue, "ورود بازه ذخیره انرژی", "", 2, IN_MODE_NUMBERS);
        } else if (energyMenu.idx == 1) {
            GOTO_INPUT(state, getValue, "ورود بازه خاموشی", "", 2, IN_MODE_NUMBERS);
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
    subSettings[SET_ITEM_ENERGY] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_ENERGY], parent, "energy settings");
    subSettings[SET_ITEM_ENERGY]->vtable.enter = STATE_ENTER(EnergySettings);
    subSettings[SET_ITEM_ENERGY]->vtable.exit = STATE_EXIT(EnergySettings);
    subSettings[SET_ITEM_ENERGY]->vtable.handleKeypad = STATE_HANDLE(EnergySettings, KeypadEvent);

    getValue = (SubState *)GET_MEM(sizeof(SubState));
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

static Menu receiptMenu;

STATE_DEF_ENTER(ReceiptSettings) {
    uiMenu(&receiptMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < 4 ; i++) {
        OOP_CALL(&receiptMenu, addItem, &receiptItemTxt[i], subReceipt[i], NULL, NULL);
    }
    GOTO_MENU(state->parent, &receiptMenu, NULL);
}

static Menu autoRecMenu;

STATE_DEF_ENTER(AutoPrint) {
    uiOnOffMenu(&autoRecMenu, getDisplay()->screen);
    OOP_CALL(&autoRecMenu, show);
}

STATE_DEF_EXIT(AutoPrint) {
    OOP_CALL(&autoRecMenu, hide);
    uiDeleteMenu(&autoRecMenu);
}

STATE_DEF_HANDLE(AutoPrint, KeypadEvent) {
    OOP_CALL(&autoRecMenu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        OOP_CALL(&autoRecMenu, setChecked, autoRecMenu.idx);
    }
}

static SubState *secPrintSuc;

STATE_DEF_ENTER(SecPrintTime) {
    GOTO_INPUT(state->parent, secPrintSuc, "زمان رسید دوم", "", 2, IN_MODE_NUMBERS);
}

STATE_DEF_ENTER(SecPrintTimeSuc) {
    GOTO_INFO(state->parent, state->parent, "با موفقیت انجام شد", "");
}

static Menu merchRecMenu;

STATE_DEF_ENTER(PrnMerchRec) {
    uiOnOffMenu(&merchRecMenu, getDisplay()->screen);
    OOP_CALL(&merchRecMenu, show);
}

STATE_DEF_EXIT(PrnMerchRec) {
    OOP_CALL(&merchRecMenu, hide);
    uiDeleteMenu(&merchRecMenu);
}

STATE_DEF_HANDLE(PrnMerchRec, KeypadEvent) {
    OOP_CALL(&merchRecMenu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        OOP_CALL(&merchRecMenu, setChecked, merchRecMenu.idx);
    }
}

static Menu prnModel;

STATE_DEF_ENTER(PrnModel) {
    uiMenu(&prnModel, getDisplay()->screen);
    OOP_CALL(&prnModel, addItem, "پس زمینه سفید", NULL, NULL, NULL);
    OOP_CALL(&prnModel, addItem, "پس زمینه مشکی", NULL, NULL, NULL);
    OOP_CALL(&prnModel, show);
}

STATE_DEF_EXIT(PrnModel) {
    OOP_CALL(&prnModel, hide);
    uiDeleteMenu(&prnModel);
}

STATE_DEF_HANDLE(PrnModel, KeypadEvent) {
    OOP_CALL(&prnModel, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        OOP_CALL(&prnModel, setChecked, prnModel.idx);
    }
}

static void ReceiptSettings(State *parent) {
    subSettings[SET_ITEM_RECEIPT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_RECEIPT], parent, "receipt settings");
    subSettings[SET_ITEM_RECEIPT]->vtable.enter = STATE_ENTER(ReceiptSettings);

    subReceipt[0] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[0], subSettings[SET_ITEM_RECEIPT], "auto print");
    subReceipt[0]->vtable.enter = STATE_ENTER(AutoPrint);
    subReceipt[0]->vtable.exit = STATE_EXIT(AutoPrint);
    subReceipt[0]->vtable.handleKeypad = STATE_HANDLE(AutoPrint, KeypadEvent);

    subReceipt[1] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[1], subSettings[SET_ITEM_RECEIPT], "sec print time");
    subReceipt[1]->vtable.enter = STATE_ENTER(SecPrintTime);

    secPrintSuc = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, secPrintSuc, subSettings[SET_ITEM_RECEIPT], "sec print time suc");
    secPrintSuc->vtable.enter = STATE_ENTER(SecPrintTimeSuc);

    subReceipt[2] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[2], subSettings[SET_ITEM_RECEIPT], "print merchaant receipt");
    subReceipt[2]->vtable.enter = STATE_ENTER(PrnMerchRec);
    subReceipt[2]->vtable.exit = STATE_EXIT(PrnMerchRec);
    subReceipt[2]->vtable.handleKeypad = STATE_HANDLE(PrnMerchRec, KeypadEvent);

    subReceipt[3] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subReceipt[3], subSettings[SET_ITEM_RECEIPT], "print model");
    subReceipt[3]->vtable.enter = STATE_ENTER(PrnModel);
    subReceipt[3]->vtable.exit = STATE_EXIT(PrnModel);
    subReceipt[3]->vtable.handleKeypad = STATE_HANDLE(PrnModel, KeypadEvent);
}

/******************** screen light sub state **********************/
static Bar brightBar;

STATE_DEF_ENTER(ScrLightSettings) {
    uiBar(&brightBar, getDisplay()->screen, 1, dev->maxBright);
    OOP_CALL(&brightBar, setTitle, "تنظیم نور صفحه");
    OOP_CALL(&brightBar, setValue, storage->settings->terminal.brightness);
    OOP_CALL(&brightBar, show);
}

STATE_DEF_EXIT(ScrLightSettings) {
    storage->settings->terminal.brightness = brightBar.value;
    SAVE_SETTINGS();
    OOP_CALL(&brightBar, hide);
    uiBarDelete(&brightBar);
}

STATE_DEF_HANDLE(ScrLightSettings, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {

    } else if (ev->key == KEY_UP) {
        OOP_CALL(&brightBar, increase);
    } else if (ev->key == KEY_DOWN) {
        OOP_CALL(&brightBar, decrease);
    }
    OOP_CALL(dev, setBrightness, brightBar.value);
}

static void ScrLightSettings(State *parent) {
    subSettings[SET_ITEM_SCR_LIGHT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_SCR_LIGHT], parent, "receipt settings");
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.enter = STATE_ENTER(ScrLightSettings);
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.exit = STATE_EXIT(ScrLightSettings);
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.handleKeypad = STATE_HANDLE(ScrLightSettings, KeypadEvent);
}

/******************** touch sub state **********************/

static Menu touchMenu;

STATE_DEF_ENTER(TouchSettings) {
    uiOnOffMenu(&touchMenu, getDisplay()->screen);
    int idx = storage->settings->terminal.mTouchEnable == true ? 0 : 1;
    OOP_CALL(&touchMenu, setChecked, idx);
    OOP_CALL(&touchMenu, show);
}

STATE_DEF_EXIT(TouchSettings) {
    OOP_CALL(&touchMenu, hide);
    uiDeleteMenu(&touchMenu);
}

STATE_DEF_HANDLE(TouchSettings, KeypadEvent) {
    OOP_CALL(&touchMenu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        bool en = touchMenu.idx == 0;
        //TODO: enable/ disable touch
        storage->settings->terminal.mTouchEnable = en;
        OOP_CALL(&touchMenu, setChecked, touchMenu.idx);
    }
}

static void TouchSettings(State *parent) {
    subSettings[SET_ITEM_TOUCH] = (SubState *)GET_MEM(sizeof(SubState));
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
    subSettings[SET_ITEM_DATE_TIME] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_DATE_TIME], parent, "date time settings");
    subSettings[SET_ITEM_DATE_TIME]->vtable.enter = STATE_ENTER(DateTimeSettings);
    subSettings[SET_ITEM_DATE_TIME]->vtable.exit = STATE_EXIT(DateTimeSettings);
    subSettings[SET_ITEM_DATE_TIME]->vtable.handleKeypad = STATE_HANDLE(DateTimeSettings, KeypadEvent);
}

/******************** Settings sub state **********************/
static Menu settingsMenu;

static void createUi() {
    uiMenu(&settingsMenu, getDisplay()->screen);
    for (uint8_t i = 0; i < SET_ITEM_ALL ; i++) {
        OOP_CALL(&settingsMenu, addItem, SettingsItemTxt[i], subSettings[i], NULL, NULL);
    }
}

STATE_DEF_ENTER(Settings) {
    createUi();
    GOTO_MENU(state->parent, &settingsMenu, NULL);
}

OOP_CTOR(Settings, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Settings);
    storage = getStorage();
    dev = getDevice();
    SoundSettings(self);
    EnergySettings(self);
    ReceiptSettings(self);
    ScrLightSettings(self);
    TouchSettings(self);
    DateTimeSettings(self);
}