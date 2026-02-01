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

#define INPUT_MAX_LEN 50

static InputBox inputBox;
static lv_obj_t *confirmBut;
static lv_obj_t *cancelBut;

static InputMode_t inMode;
static char *input;
static char *amountStr;

STATE_DEF_ENTER(Input) {
    LV_SHOW(inputBox.main);
    LV_SHOW(confirmBut);
    LV_SHOW(cancelBut);
    clearStr(input);
}

STATE_DEF_EXIT(Input) {
    LV_HIDE(inputBox.main);
    LV_HIDE(confirmBut);
    LV_HIDE(cancelBut);
    LV_SET_TEXT(inputBox.textBox, "");
    // TODO: pass state to enter and exit method too
}

STATE_DEF_HANDLE(TimeOutEvent) {

}

static void handleAmountInput(KeypadEvent *ev) {
    if (ev->key == KEY_CLEAR) {
        deleteChar(input);
    }  else {
        appendChar(input, INPUT_MAX_LEN, ev->keyStr);
    }
    int ret = amountSeparator(input, amountStr, INPUT_MAX_LEN);
    LV_SET_TEXT(inputBox.textBox, amountStr);
}

STATE_DEF_HANDLE(KeypadEvent) {
    switch (ev->key) {
    case KEY_ESC:
        if (state->prev == NULL) {
            LOG_WARN("Input state: previous state is not set.");
        } else {
            SM_GOTO(state->prev );
        }
        break;
    case KEY_ENTER:
        if (state->next == NULL) {
            LOG_WARN("Input state: next state is not set.");
        } else {
            SM_GOTO(state->next);
        }
        break;
    default:
        if (ev->key <= KEY_9 || ev->key == KEY_CLEAR) {
            switch (inMode) {
            case IN_MODE_AMOUNT:
                handleAmountInput(ev);
                break;
            
            default:
                break;
            }
        }
        break;
    }
}

static void createUi() {
    inputBox = uiInputBox(getDisplay()->screen);
    LV_ALIGN(inputBox.main, LV_ALIGN_CENTER, 0, 10);
    confirmBut = uiConfirmButton(getDisplay()->screen);
    LV_ALIGN(confirmBut, LV_ALIGN_BOTTOM_RIGHT, -5, -15);
    cancelBut = uiCancellButton(getDisplay()->screen);
    LV_ALIGN(cancelBut, LV_ALIGN_BOTTOM_LEFT, 5, -15);

    LV_SET_TEXT(inputBox.textBox, "");
}

void setMode(InputMode_t mode) {
    inMode = mode;
}

OOP_CTOR(Input, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Input);
    self->base.vtable.exit = STATE_EXIT(Input);
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->setMode = setMode;
    input = (char*)GET_MEM(INPUT_MAX_LEN);
    amountStr = (char*)GET_MEM(INPUT_MAX_LEN);
    createUi();
}