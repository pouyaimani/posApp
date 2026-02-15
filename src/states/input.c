#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "dev.h"
#include "mylvgl.h"
#include "display.h"
#include "wifi/wifi.h"
#include "ui/ui.h"
#include "utility/utility.h"

#define INPUT_MAX_LEN       50
#define PASS_MAX_LEN        4
#define AMOUNT_MAX_LEN      10

static InputBox inputBox;
static Button confirmBut;
static Button cancelBut;
static lv_obj_t *title;
static lv_obj_t *info;

static InputMode_t inMode;
static char *input;
static char *amountStr;
static uint8_t idx = 0;
static uint8_t maxIn = 0;

STATE_DEF_ENTER(Input) {
    LV_SHOW(inputBox.main);
    LV_SHOW(confirmBut.main);
    LV_SHOW(cancelBut.main);
    LV_SHOW(title);
    LV_SHOW(info);
}

STATE_DEF_EXIT(Input) {
    LV_HIDE(inputBox.main);
    LV_HIDE(confirmBut.main);
    LV_HIDE(cancelBut.main);
    LV_HIDE(title);
    LV_HIDE(info);
    // TODO: pass state to enter and exit method too
}

STATE_DEF_HANDLE(Input, TimeOutEvent) {

}

static void showMaxError(State *state) {
    switch (inMode) {
    case IN_MODE_AMOUNT:
        {
        SHOW_INFO(state, "خطا", "مبلغ بیش از حد مجاز");
        }
        break;
    case IN_MODE_PASSWORD:
        break;
    case IN_MODE_NUMBERS:
        {
        SHOW_INFO(state, "خطا", "ورودی بیش از حد مجاز");
        }
        break;
    default:
        break;
    }
}

static void handleInput(State *state, KeypadEvent *ev)
{
    bool isPassword = inMode == IN_MODE_PASSWORD ? true : false;
    bool isAmount = inMode == IN_MODE_AMOUNT ? true : false;
    if (ev->key == KEY_CLEAR) {
        deleteChar(input);
    } else {
        if (idx >= maxIn) {
            showMaxError(state);
            return;
        }
        appendChar(input, INPUT_MAX_LEN,
                   isPassword ? '*' : ev->keyStr);
    }

    if (isAmount) {
        amountSeparator(input, amountStr, INPUT_MAX_LEN);
        LV_SET_TEXT(inputBox.textBox, amountStr);
    } else {
        LV_SET_TEXT(inputBox.textBox, input);
    }
}

STATE_DEF_HANDLE(Input, KeypadEvent)
{
    if (ev->key == KEY_ESC) {
        if (state->prev)
            SM_GOTO(state->prev);
        else
            LOG_WARN("Input state: previous state is not set.");
        return;
    } else if (ev->key == KEY_ENTER) {
        if (state->next) {
            if (inMode == IN_MODE_PASSWORD) {
                if (idx != maxIn) {
                    return;
                }
            }
            SM_GOTO(state->next);
        } else
            LOG_WARN("Input state: next state is not set.");
    } 

    if (ev->key > KEY_9 && ev->key != KEY_CLEAR )
        return;

    handleInput(state, ev);
    if (ev->key <= KEY_9 && idx < maxIn) {
        idx++;
    } else if (ev->key == KEY_CLEAR && idx > 0) {
        idx--;
    }
    LOG_TRACE("idx = %d", idx);
}


static void createUi() {
    title = lv_label_create(getDisplay()->screen);
    LV_SET_TEXT_FONT(title, FONT_20);
    LV_SET_TEXT_COLOR(title, 0xFF4E4E);
    LV_SET_SIZE(title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(title, LV_ALIGN_CENTER, 0, -70);

    info = lv_label_create(getDisplay()->screen);
    LV_SET_TEXT_FONT(info, FONT_16);
    LV_SET_TEXT_COLOR(info, 0x333333);
    LV_SET_SIZE(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(info, LV_ALIGN_CENTER, 0, -30);
    
    inputBox = uiInputBox(getDisplay()->screen);
    LV_ALIGN(inputBox.main, LV_ALIGN_CENTER, 0, 10);
    confirmBut = uiButton(getDisplay()->screen, 0x68DD40, "تایید");
    LV_ALIGN(confirmBut.main, LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    cancelBut = uiButton(getDisplay()->screen, 0xFF4E4E, "لغو");
    LV_ALIGN(cancelBut.main, LV_ALIGN_BOTTOM_LEFT, 5, -10);

    LV_SET_TEXT(inputBox.textBox, "");
}

static void setMode(InputMode_t mode) {
    inMode = mode;
}

static void setData(const char *dtitle, const char *dinfo) {
    LV_SET_TEXT(title, dtitle);
    LV_SET_TEXT(info, dinfo);
}

static void setMax(int val) {
    maxIn = val;
}

static void reset() {
    clearStr(input);
    idx = 0;
    LV_SET_TEXT(inputBox.textBox, "");
    LV_SET_TEXT(title, "");
    LV_SET_TEXT(info, "");
}

OOP_CTOR(Input, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Input);
    self->base.vtable.exit = STATE_EXIT(Input);
    self->base.vtable.handleKeypad = STATE_HANDLE(Input, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Input, TimeOutEvent);
    self->setMode = setMode;
    self->setData = setData;
    self->setMax = setMax;
    self->reset = reset;
    input = (char*)GET_MEM(INPUT_MAX_LEN);
    self->input = input;
    amountStr = (char*)GET_MEM(INPUT_MAX_LEN);
    createUi();
}