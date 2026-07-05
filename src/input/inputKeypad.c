#include "inputProvider.h"
#include "eventloop.h"
#include "event.h"
#include "inputCtrl.h"
#include "error.h"
#include "common.h"
#include "utility/alphabetic.h"
#include "phrases/phrases.h"

typedef struct {
    InputMode mode;
    InputResult (*handle)(KeypadEvent* ev);
} KeypadHandler;

extern InputController keypadCtrl;

static inline uint8_t maxLen(void) { return keypadCtrl.cfg.maxLen; };

static const char* input  = keypadCtrl.input;
static const char* finput = keypadCtrl.finput;

static const KeypadHandler* findHandler(InputMode mode);

static bool isNumeric(Key_t key) {
    return ((key <= KEY_9) && (key != KEY_NONE));
}

static bool poll(InputProvider* self, InputEvent* ev) {}

static bool start() { return true; }

static void stop() {}

static InputResult handleKeypadEv(InputEvent* ev, InputCfg cfg) {
    if (ev->keypad.key == KEY_ESC) {
        return INPUT_RES_CANCELED;
    } else if (ev->keypad.key == KEY_ENTER) {
        return INPUT_RES_FINISHED;
    }
    if (ev->keypad.key == KEY_CLEAR) {
        deleteChar(input);
    }
    if (isNumeric(ev->keypad.key)) {
        appendChar(input, maxLen() + 1, ev->keypad.keyStr);
    }
    KeypadHandler* handler = findHandler(cfg.mode);
    if (handler) {
        handler->handle(&ev->keypad);
    }
    return INPUT_RES_PROCESSING;
}

static void handleAmuont(KeypadEvent* ev) {
    if (input[0] == '0') {
        deleteChar(input);
    }
    amountSeparator(input, finput, maxLen());
}

static void handlePin(KeypadEvent* ev) {
    memset(finput, 0, INPUT_SIZE);
    int len = strlen(input);
    for (int i = 1; i <= len; i++) {
        appendChar(finput, maxLen() + 1, '*');
    }
}

static void handleNumbers(KeypadEvent* ev) { strcpy(finput, input); }

static void handleAlphaB(KeypadEvent* ev) {
    if (ev->key == KEY_CLEAR) {
        return;
    }
    deleteChar(input);
    alphebatic()->addKey(input, maxLen(), ev->key);
}

static void handleTime(KeypadEvent* ev) { timeFormat(input, finput); }

static void handleDate(KeypadEvent* ev) { dateFormat(input, finput); }

static void handleIp(KeypadEvent* ev) { ipFormatLeftAligned(input, finput); }

static const KeypadHandler keypadHandlers[] = {
    {.mode = INMD_ENTER_AMOUNT, .handle = handleAmuont},
    {.mode = INMD_ENTER_PIN, .handle = handlePin},
    {.mode = INMD_ENTER_NUMBERS, .handle = handleNumbers},
    {.mode = INMD_ENTER_ALPHAB, .handle = handleAlphaB},
    {.mode = INMD_ENTER_TIME, .handle = handleTime},
    {.mode = INMD_ENTER_DATE, .handle = handleDate},
    {.mode = INMD_ENTER_IP, .handle = handleIp},
};

static const InputProvider keypadProvider = {

    .start = start,

    .poll = poll,

    .stop = stop};

static const InputCb keypadCb = {
    .display = NULL, .handle = handleKeypadEv, .reset = NULL, .validate = NULL};

InputController keypadCtrl = {.provider = &keypadProvider, .cb = &keypadCb};

static const KeypadHandler* findHandler(InputMode mode) {
    for (size_t i = 0; i < ARRAY_SIZE(keypadHandlers); i++) {
        if (keypadHandlers[i].mode == mode) {
            return &keypadHandlers[i];
        }
    }
    return NULL;
}

void inputKeypadInit(InputController* c) {}