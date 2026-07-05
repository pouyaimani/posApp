
#include "inputMgr.h"
#include "logger.h"
#include "inputCtrl.h"
#include "mylvgl.h"
#include "display.h"
#include "ui/inbox.h"
#include "ui/button.h"
#include "utility/utility.h"
#include "phrases/phrases.h"
#include "font/myFont.h"
#include "ui/infoPage.h"
#include "sys/sys.h"
#include "error.h"

static InputMgr __inputmgr;

static InputBox  inputBox;
static Button    confirmBut;
static Button    cancelBut;
static lv_obj_t* title;
static lv_obj_t* info;

static void ui_show(InputController* inCtrl) {
    LV_SET_TEXT(inputBox.textBox, inCtrl->finput);
    LV_SET_TEXT(title, inCtrl->cfg.title);
    LV_SET_TEXT(info, inCtrl->cfg.info);
    LV_SHOW(inputBox.main);
    LV_SHOW(ui_button_obj(&confirmBut));
    LV_SHOW(ui_button_obj(&cancelBut));
    LV_SHOW(title);
    LV_SHOW(info);
}

static void ui_hide() {
    LV_HIDE(inputBox.main);
    LV_HIDE(ui_button_obj(&confirmBut));
    LV_HIDE(ui_button_obj(&cancelBut));
    LV_HIDE(title);
    LV_HIDE(info);
}

static dummyUpdate() {
    InputEvent event;
    if (__inputmgr.ctrl->cfg.type == INPUT_TYPE_KEYPAD) {
        event.type       = INPUT_EV_KEYPAD;
        event.keypad.key = KEY_NONE;
    } else if (__inputmgr.ctrl->cfg.type == INPUT_TYPE_PED) {
        event.type     = INPUT_EV_PED;
        event.ped.type = PED_DUMMY;
    }
    inputUpdate(__inputmgr.ctrl, &event);
}

STATE_DEF_ENTER(Input) {
    __inputmgr.input  = __inputmgr.ctrl->input;
    __inputmgr.finput = __inputmgr.ctrl->finput;
    __inputmgr.ctrl->provider->start();
    dummyUpdate();
    ui_show(__inputmgr.ctrl);
}

STATE_DEF_EXIT(Input) {
    __inputmgr.ctrl->provider->stop();
    if (__inputmgr.oraw) {
        snprintf(__inputmgr.oraw, __inputmgr.outlen, "%s", __inputmgr.input);
    }
    if (__inputmgr.oformated) {
        snprintf(__inputmgr.oformated, __inputmgr.outlen, "%s",
                 __inputmgr.finput);
    }
    ui_hide();
}

STATE_DEF_HANDLE(Input, KeypadEvent) {
    if (__inputmgr.ctrl->cfg.type != INPUT_TYPE_KEYPAD) {
        return;
    }
    InputEvent event;
    event.type      = INPUT_EV_KEYPAD;
    event.keypad    = *ev;
    InputResult res = inputUpdate(__inputmgr.ctrl, &event);
    if (res == INPUT_RES_FINISHED) {
        SM_GOTO(__inputmgr.next);
        return;
    } else if (res == INPUT_RES_CANCELED) {
        SM_GOTO(__inputmgr.back);
        return;
    }
    ui_show(__inputmgr.ctrl);
}

STATE_DEF_HANDLE(Input, PedEvent) {
    if (__inputmgr.ctrl->cfg.type != INPUT_TYPE_PED) {
        return;
    }
    InputEvent input;
    input.type      = INPUT_EV_PED;
    input.ped       = *ev;
    InputResult res = inputUpdate(__inputmgr.ctrl, &input);
    if (res == INPUT_RES_FINISHED) {
        SM_GOTO(__inputmgr.next);
        return;
    } else if (res == INPUT_RES_CANCELED) {
        SM_GOTO(__inputmgr.back);
        return;
    }
    ui_show(__inputmgr.ctrl);
}

STATE_DEF_HANDLE(Input, TimeOutEvent) {
    InputEvent input;
    input.type = INPUT_EV_TIMEOUT;
    inputUpdate(NULL, &input);
}

static void ui_create() {
    title = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(title, FONT_20);
    LV_SET_TEXT_COLOR(title, COLOR_TEXT_PRIMARY);
    LV_SET_SIZE(title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(title, LV_ALIGN_CENTER, 0, -70);

    info = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(info, FONT_16);
    LV_SET_TEXT_COLOR(info, COLOR_TEXT_SECONDARY);
    LV_SET_SIZE(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(info, LV_ALIGN_CENTER, 0, -30);

    ui_inBox_create(&inputBox, disp()->screen);
    LV_ALIGN(inputBox.main, LV_ALIGN_CENTER, 0, 10);
    ui_button_create(&confirmBut, disp()->screen);
    ui_button_set_text(&confirmBut, phraseGetDef(PHRASE_CONFIRM));
    ui_button_set_icon(&confirmBut, LV_SYMBOL_OK);
    ui_button_set_color(&confirmBut, lv_color_hex(0x68DD40));
    LV_ALIGN(ui_button_obj(&confirmBut), LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    ui_button_create(&cancelBut, disp()->screen);
    ui_button_set_text(&cancelBut, phraseGetDef(PHRASE_CANCEL));
    ui_button_set_icon(&cancelBut, LV_SYMBOL_CLOSE);
    ui_button_set_color(&cancelBut, lv_color_hex(0xFF4E4E));
    LV_ALIGN(ui_button_obj(&cancelBut), LV_ALIGN_BOTTOM_LEFT, 5, -10);

    LV_SET_TEXT(inputBox.textBox, "");
}

static void reset(InputType type) {
    InputController* ctrl;
    if (type == INPUT_TYPE_KEYPAD) {
        ctrl = &keypadCtrl;
    } else if (type == INPUT_TYPE_PED) {
        ctrl = &pedCtrl;
    } else {
        return;
    }
    memset(ctrl->input, 0, sizeof(ctrl->input));
    memset(ctrl->finput, 0, sizeof(ctrl->finput));
}

static void set(InputType type, const char* in) {
    InputController* ctrl;
    if (type == INPUT_TYPE_KEYPAD) {
        ctrl = &keypadCtrl;
    } else if (type == INPUT_TYPE_PED) {
        ctrl = &pedCtrl;
    } else {
        return;
    }
    memset(ctrl->input, 0, INPUT_SIZE);
    strcpy(ctrl->input, in);
}

void setOut(char* oraw, char* oformated, uint16_t len) {
    __inputmgr.oraw      = oraw;
    __inputmgr.oformated = oformated;
    __inputmgr.outlen    = len;
}

static void run(InputCfg* cfg, State* back, State* next) {
    reset(cfg->type);
    __inputmgr.oraw      = NULL;
    __inputmgr.oformated = NULL;
    if (cfg->type == INPUT_TYPE_KEYPAD) {
        __inputmgr.ctrl = &keypadCtrl;
    } else if (cfg->type == INPUT_TYPE_PED) {
        __inputmgr.ctrl = &pedCtrl;
    }
    __inputmgr.ctrl->cfg = *cfg;
    __inputmgr.next      = next;
    __inputmgr.back      = back;
    SM_GOTO(__inputmgr.state);
}

InputMgr* inmgr() {
    CALL_ONCE(__inputmgr.state = MEM_ALLOC(sizeof(State)); __inputmgr.run = run;
              __inputmgr.setOut = setOut; __inputmgr.set = set;
              __inputmgr.reset                           = reset;
              OOP_CALL_CTOR(State, __inputmgr.state, NULL, "");
              __inputmgr.state->vtable.enter = STATE_ENTER(Input);
              __inputmgr.state->vtable.exit  = STATE_EXIT(Input);
              __inputmgr.state->vtable.handleKeypad =
                  STATE_HANDLE(Input, KeypadEvent);
              __inputmgr.state->vtable.handlePed =
                  STATE_HANDLE(Input, PedEvent);
              __inputmgr.state->vtable.handleTimeout =
                  STATE_HANDLE(Input, TimeOutEvent);
              inputInit(&keypadCtrl); inputInit(&pedCtrl); ui_create(););
    return &__inputmgr;
}