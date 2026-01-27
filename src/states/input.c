#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "dev.h"
#include "lvgl.h"
#include "display.h"
#include "wifi/wifi.h"
#include "ui/ui.h"
#include "utility/utility.h"

#define INPUT_MAX_LEN 50

static InputBox inputBox;
static lv_obj_t *confirmBut;
static lv_obj_t *cancelBut;

static State *nextState = NULL;
static State *prevState = NULL;

static InputMode_t inMode;
static char *input;
static char *amountStr;

STATE_DEF_ENTER() {
    lv_obj_remove_flag(inputBox.main, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(confirmBut, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(cancelBut, LV_OBJ_FLAG_HIDDEN);
    clearStr(input);
}

STATE_DEF_EXIT() {
    lv_obj_add_flag(inputBox.main, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(confirmBut, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cancelBut, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(inputBox.textBox, "");
    nextState = NULL;
    prevState = NULL;
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
    lv_label_set_text(inputBox.textBox, amountStr);
}

STATE_DEF_HANDLE(KeypadEvent) {
    switch (ev->key) {
    case KEY_ESC:
        if (prevState == NULL) {
            LOG_WARN("Input state: previous state is not set.");
        } else {
            SM_GOTO(prevState);
        }
        break;
    case KEY_ENTER:
        if (nextState == NULL) {
            LOG_WARN("Input state: next state is not set.");
        } else {
            SM_GOTO(nextState);
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
    lv_obj_align(inputBox.main, LV_ALIGN_CENTER, 0, 10);
    confirmBut = uiConfirmButton(getDisplay()->screen);
    lv_obj_align(confirmBut, LV_ALIGN_BOTTOM_RIGHT, -5, -15);
    cancelBut = uiCancellButton(getDisplay()->screen);
    lv_obj_align(cancelBut, LV_ALIGN_BOTTOM_LEFT, 5, -15);

    lv_label_set_text(inputBox.textBox, "");
}

static void setPrevState(State *state) {
    prevState = state;
}

static void setNextState(State *state) {
    nextState = state;
}

void setMode(InputMode_t mode) {
    inMode = mode;
}

OOP_CTOR(Input, State *parent, const char *name) {
    State_ctor(self, parent, name);
    self->base.vtable.enter = STATE_ENTER();
    self->base.vtable.exit = STATE_EXIT();
    self->base.vtable.handleKeypad = STATE_HANDLE(KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(TimeOutEvent);
    self->setPrevState = setPrevState;
    self->setNextState = setNextState;
    self->setMode = setMode;
    input = (char*)GET_MEM(INPUT_MAX_LEN);
    amountStr = (char*)GET_MEM(INPUT_MAX_LEN);
    createUi();
}