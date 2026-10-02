#include "inputProvider.h"
#include "inputCtrl.h"
#include "ped/ped.h"
#include "keypad/keypad.h"

#define PED_DUMMY '0'

static inline uint8_t maxLen(void) { return pedCtrl.cfg.maxLen; };

static const char* input  = pedCtrl.input;
static const char* finput = pedCtrl.finput;

static uint8_t len;

static bool start() {
    LOG_TRACE("input provider started");
    keypad()->unregisterIo();
    RETURN_IF_NOT(ped()->enterPinEntryMode(), PED_ERR_OK, ;);
}

static bool poll(InputProvider* self, InputEvent* ev) {}

static void stop() {
    LOG_TRACE("input provider stoped");
    keypad()->registerIo();
    RETURN_IF_NOT(ped()->exitPinEntryMode(), PED_ERR_OK, ;);
}

static InputResult handlePedEv(InputEvent* ev, InputCfg cfg) {
    if (ev->ped.type == PED_CANCEL) {
        return INPUT_RES_CANCELED;
    } else if (ev->ped.type == PED_ENTER) {
        return INPUT_RES_FINISHED;
    } else if (ev->ped.type == PED_CLEAR) {
        deleteChar(finput);
    } else if (ev->ped.type == PED_DIGIT) {
        appendChar(finput, maxLen() + 1, '*');
    }
    return INPUT_RES_PROCESSING;
}

InputProvider pedProvider = {

    .start = start,

    .poll = poll,

    .stop = stop};

InputCb pedCb = {
    .display = NULL, .handle = handlePedEv, .reset = NULL, .validate = NULL};

InputController pedCtrl = {.provider = &pedProvider, .cb = &pedCb};