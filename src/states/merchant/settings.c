#include "merchant.h"
#include "states/states.h"
#include "logger.h"
#include "display.h"
#include "event.h"
#include "ui/ui.h"
#include "dev/dev.h"

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
    "به روز رسیانی زمان و تاریخ",
};

/******************** sound sub state **********************/

STATE_DEF_ENTER(SoundSettings) {

}

STATE_DEF_EXIT(SoundSettings) {

}

static void SoundSettings(State *parent) {
    subSettings[SET_ITEM_SOUND] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_SOUND], parent, "sound settings");
    subSettings[SET_ITEM_SOUND]->vtable.enter = STATE_ENTER(SoundSettings);
    subSettings[SET_ITEM_SOUND]->vtable.exit = STATE_EXIT(SoundSettings);
}

/******************** energy sub state **********************/

STATE_DEF_ENTER(EnergySettings) {

}

STATE_DEF_EXIT(EnergySettings) {

}

static void EnergySettings(State *parent) {
    subSettings[SET_ITEM_ENERGY] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_ENERGY], parent, "energy settings");
    subSettings[SET_ITEM_ENERGY]->vtable.enter = STATE_ENTER(EnergySettings);
    subSettings[SET_ITEM_ENERGY]->vtable.exit = STATE_EXIT(EnergySettings);
}

/******************** receipt sub state **********************/

STATE_DEF_ENTER(ReceiptSettings) {

}

STATE_DEF_EXIT(ReceiptSettings) {

}

static void ReceiptSettings(State *parent) {
    subSettings[SET_ITEM_RECEIPT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_RECEIPT], parent, "receipt settings");
    subSettings[SET_ITEM_RECEIPT]->vtable.enter = STATE_ENTER(ReceiptSettings);
    subSettings[SET_ITEM_RECEIPT]->vtable.exit = STATE_EXIT(ReceiptSettings);
}

/******************** screen light sub state **********************/

STATE_DEF_ENTER(ScrLightSettings) {

}

STATE_DEF_EXIT(ScrLightSettings) {

}

static void ScrLightSettings(State *parent) {
    subSettings[SET_ITEM_SCR_LIGHT] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_SCR_LIGHT], parent, "receipt settings");
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.enter = STATE_ENTER(ScrLightSettings);
    subSettings[SET_ITEM_SCR_LIGHT]->vtable.exit = STATE_EXIT(ScrLightSettings);
}

/******************** touch sub state **********************/

STATE_DEF_ENTER(TouchSettings) {

}

STATE_DEF_EXIT(TouchSettings) {

}

static void TouchSettings(State *parent) {
    subSettings[SET_ITEM_TOUCH] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_TOUCH], parent, "receipt settings");
    subSettings[SET_ITEM_TOUCH]->vtable.enter = STATE_ENTER(TouchSettings);
    subSettings[SET_ITEM_TOUCH]->vtable.exit = STATE_EXIT(TouchSettings);
}

/******************** date time sub state **********************/

STATE_DEF_ENTER(DateTimeSettings) {

}

STATE_DEF_EXIT(DateTimeSettings) {

}

static void DateTimeSettings(State *parent) {
    subSettings[SET_ITEM_DATE_TIME] = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, subSettings[SET_ITEM_DATE_TIME], parent, "receipt settings");
    subSettings[SET_ITEM_DATE_TIME]->vtable.enter = STATE_ENTER(DateTimeSettings);
    subSettings[SET_ITEM_DATE_TIME]->vtable.exit = STATE_EXIT(DateTimeSettings);
}

/******************** Settings sub state **********************/
static Menu settingsMenu;

static void createUi() {
    settingsMenu = uiMenu(getDisplay()->screen);
    for (uint8_t i = 0; i < SET_ITEM_ALL ; i++) {
        OOP_CALL(&settingsMenu, addItem, SettingsItemTxt[i], NULL, NULL);
    }
}

static void destroyUi() {
    uiDeleteMenu(&settingsMenu);
}

STATE_DEF_ENTER(Settings) {
    createUi();
    OOP_CALL(&settingsMenu, show);
}

STATE_DEF_EXIT(Settings) {
    OOP_CALL(&settingsMenu, hide);
    destroyUi();
}

static void handleKeyAction(State *state, int id) {
    if (id >= SET_ITEM_ALL) {
        return;
    }
}

STATE_DEF_HANDLE(Settings, KeypadEvent) {
    OOP_CALL(&settingsMenu, handleItem, ev->key);
    if (ev->key == KEY_ESC) {
        SM_GOTO(state->parent);
    } else if (ev->key == KEY_ENTER) {
        handleKeyAction(state, settingsMenu.idx);
    }  else {
        if (ev->key <= KEY_9) {
            int id = ((int)ev->key - 1);
            handleKeyAction(state, id);
        }
    }
}

OOP_CTOR(Settings, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Settings);
    self->base.vtable.exit = STATE_EXIT(Settings);
    self->base.vtable.handleKeypad = STATE_HANDLE(Settings, KeypadEvent);

    SoundSettings(self);
    EnergySettings(self);
    ReceiptSettings(self);
    ScrLightSettings(self);
    TouchSettings(self);
    DateTimeSettings(self);
}