#include "inputMgr.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "ui/inbox.h"
#include "ui/button.h"
#include "utility/utility.h"
#include "phrases/phrases.h"
#include "font/myFont.h"
#include "sys/sys.h"
#include "error.h"

static InputMgr manager; /**< Singleton public interface and current request. */
static bool initialized; /**< True after state, controllers, and UI exist. */
static bool active;      /**< Request accepted and not yet completed/exited. */
static bool entered; /**< Input state is current; prevents same-state reruns. */
static bool
    providerStarted; /**< Provider start succeeded and needs one stop. */

static InputBox  inputBox;   /**< Widget displaying formatted input. */
static Button    confirmBut; /**< Existing visual confirmation affordance. */
static Button    cancelBut;  /**< Existing visual cancellation affordance. */
static lv_obj_t* title;      /**< Request title label. */
static lv_obj_t* info;       /**< Request help/instruction label. */

InputFinishCtx
    inFinishCtx; /**< Optional shared application callback context. */

/** Return the controller for a supported input source. */
static InputController* controllerFor(InputType type) {
    switch (type) {
    case INPUT_TYPE_KEYPAD:
        return &keypadCtrl;
    case INPUT_TYPE_PED:
        return &pedCtrl;
    default:
        return NULL;
    }
}

/** Copy at most sourceCapacity bytes of readable text and always terminate dst.
 * memmove permits a source that overlaps the individual destination. */
static void copyText(char* dst, size_t capacity, const char* src,
                     size_t sourceCapacity) {
    size_t length = 0;
    if (!dst || capacity == 0)
        return;
    if (src) {
        while (length < sourceCapacity && length < capacity - 1 &&
               src[length]) {
            ++length;
        }
        memmove(dst, src, length);
    }
    dst[length] = '\0';
}

/** Adapter from a terminal input result to application state navigation. */
void inputComplete(InputResult result, void* userData) {
    const InputFinishCtx* ctx = (const InputFinishCtx*)userData;
    if (!ctx)
        return;
    if (result != INPUT_RES_FINISHED && result != INPUT_RES_CANCELED)
        return;

    /* Snapshot before navigation: the destination may reuse the shared context.
     */
    State* next = result == INPUT_RES_FINISHED ? ctx->onFinish : ctx->onCancel;
    CallBack_t callback     = ctx->cb;
    void*      callbackData = ctx->userData;
    if (next)
        SM_GOTO(next);
    if (callback)
        callback(callbackData);
}

/** Refresh labels and reveal the input-screen objects. */
static void ui_show(void) {
    InputController* ctrl = manager.ctrl;
    if (!ctrl)
        return;
    LV_SET_TEXT(inputBox.textBox, ctrl->finput);
    LV_SET_TEXT(title, ctrl->cfg.title ? ctrl->cfg.title : "");
    LV_SET_TEXT(info, ctrl->cfg.info ? ctrl->cfg.info : "");
    LV_SHOW(inputBox.main);
    LV_SHOW(ui_button_obj(&confirmBut));
    LV_SHOW(ui_button_obj(&cancelBut));
    LV_SHOW(title);
    LV_SHOW(info);
}

/** Hide the manager-owned UI. Safe to repeat after creation. */
static void ui_hide(void) {
    LV_HIDE(inputBox.main);
    LV_HIDE(ui_button_obj(&confirmBut));
    LV_HIDE(ui_button_obj(&cancelBut));
    LV_HIDE(title);
    LV_HIDE(info);
}

/** Stop a successfully started provider exactly once and hide its screen. */
static void stopSession(void) {
    if (providerStarted) {
        providerStarted = false;
        manager.ctrl->provider->stop();
    }
    ui_hide();
}

/** Remove borrowed completion/output references without erasing controller
 * data. */
static void detachRequest(void) {
    manager.cb        = NULL;
    manager.cbData    = NULL;
    manager.oraw      = NULL;
    manager.oformated = NULL;
    manager.outlen    = 0;
}

/** Finish once, clean up first, then notify. Timeout uses cfg.tocb when
 * present. A configured timeout callback replaces the ordinary callback, as in
 * the original API; otherwise timeout falls back to ordinary cancellation. */
static void finish(InputResult result, bool timedOut) {
    InPutComplete callback;
    void*         callbackData;
    if (!active)
        return;
    active = false;

    callback     = manager.cb;
    callbackData = manager.cbData;
    if (timedOut && manager.ctrl->cfg.tocb) {
        callback     = manager.ctrl->cfg.tocb;
        callbackData = manager.ctrl->cfg.cbData;
    }

    if (result == INPUT_RES_FINISHED &&
        manager.ctrl->cfg.type == INPUT_TYPE_KEYPAD) {
        copyText(manager.oraw, manager.outlen, manager.ctrl->input,
                 sizeof(manager.ctrl->input));
        copyText(manager.oformated, manager.outlen, manager.ctrl->finput,
                 sizeof(manager.ctrl->finput));
    }

    detachRequest();
    stopSession();
    LOG_TRACE("inputMgr: completed result=%d timeout=%d", (int)result,
              (int)timedOut);
    if (callback)
        callback(result, callbackData);
}

/** Dispatch one real event; terminal results share the same cleanup path. */
static void handleEvent(InputEvent* event) {
    if (!active || !entered || !providerStarted || !manager.ctrl || !event)
        return;
    InputResult result = inputUpdate(manager.ctrl, event);
    if (result == INPUT_RES_FINISHED || result == INPUT_RES_CANCELED) {
        finish(result, false);
        return;
    }
    ui_show();
}

/** Refresh keypad formatting with the existing no-key convention.
 * PED needs no synthetic event: its formatted buffer is reset before entry. */
static void refreshKeypad(void) {
    if (manager.ctrl && manager.ctrl->cfg.type == INPUT_TYPE_KEYPAD) {
        InputEvent event = {0};
        event.type       = INPUT_EV_KEYPAD;
        event.keypad.key = KEY_NONE;
        (void)inputUpdate(manager.ctrl, &event);
    }
}

/** Start the selected provider on entry. Failed start must roll itself back. */
STATE_DEF_ENTER(Input) {
    entered = true;
    if (!active || !manager.ctrl)
        return;
    if (!manager.ctrl->provider->start()) {
        LOG_TRACE("inputMgr: provider start failed");
        /* No ERROR result exists in the supplied enum. Preserve the public API.
         */
        finish(INPUT_RES_CANCELED, false);
        return;
    }
    providerStarted = true;
    refreshKeypad();
    ui_show();
}

/** An external state transition also releases the provider, without
 * notification. */
STATE_DEF_EXIT(Input) {
    active  = false;
    entered = false;
    detachRequest();
    stopSession();
}

/** Forward keypad events only to an active keypad request. */
STATE_DEF_HANDLE(Input, KeypadEvent) {
    if (!ev || !manager.ctrl || manager.ctrl->cfg.type != INPUT_TYPE_KEYPAD)
        return;
    InputEvent event = {0};
    event.type       = INPUT_EV_KEYPAD;
    event.keypad     = *ev;
    handleEvent(&event);
}

/** Forward PED events only to an active PED request. */
STATE_DEF_HANDLE(Input, PedEvent) {
    if (!ev || !manager.ctrl || manager.ctrl->cfg.type != INPUT_TYPE_PED)
        return;
    InputEvent event = {0};
    event.type       = INPUT_EV_PED;
    event.ped        = *ev;
    handleEvent(&event);
}

/** Timeout is terminal cancellation with the configured timeout callback
 * policy. */
STATE_DEF_HANDLE(Input, TimeOutEvent) {
    if (entered)
        finish(INPUT_RES_CANCELED, true);
}

/** Create the existing layout once; keep it hidden until state entry. */
static void ui_create(void) {
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
    ui_button_set_color(&confirmBut, lv_palette_main(LV_PALETTE_GREEN));
    LV_ALIGN(ui_button_obj(&confirmBut), LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    ui_button_create(&cancelBut, disp()->screen);
    ui_button_set_text(&cancelBut, phraseGetDef(PHRASE_CANCEL));
    ui_button_set_icon(&cancelBut, LV_SYMBOL_CLOSE);
    ui_button_set_color(&cancelBut, lv_palette_main(LV_PALETTE_RED));
    LV_ALIGN(ui_button_obj(&cancelBut), LV_ALIGN_BOTTOM_LEFT, 5, -10);
    LV_SET_TEXT(inputBox.textBox, "");
    ui_hide();
}

/** Clear controller buffers and any provider-independent editing state. */
static void reset(InputType type) {
    InputController* ctrl = controllerFor(type);
    if (!ctrl)
        return;
    if (ctrl->cb && ctrl->cb->reset)
        ctrl->cb->reset();
    memset(ctrl->input, 0, sizeof(ctrl->input));
    memset(ctrl->finput, 0, sizeof(ctrl->finput));
    if (active && entered && ctrl == manager.ctrl)
        ui_show();
}

/** Replace raw text without overflowing its fixed-size controller buffer. */
static void set(InputType type, const char* text) {
    InputController* ctrl = controllerFor(type);
    size_t           capacity;
    if (!ctrl)
        return;
    capacity = sizeof(ctrl->input);
    if (ctrl == manager.ctrl && active && ctrl->cfg.maxLen < capacity) {
        capacity = (size_t)ctrl->cfg.maxLen + 1;
    }
    copyText(ctrl->input, capacity, text, sizeof(ctrl->input) - 1);
    if (active && entered && providerStarted && ctrl == manager.ctrl) {
        refreshKeypad();
        ui_show();
    }
}

/** Register buffers for this request; inactive calls cannot leave stale
 * pointers. */
static void commit(char* oraw, char* oformated, uint16_t len) {
    if (!active) {
        LOG_TRACE("inputMgr: commit ignored without an active request");
        return;
    }
    manager.oraw      = len ? oraw : NULL;
    manager.oformated = len ? oformated : NULL;
    manager.outlen    = len;
}

/** Configure one request. Formatting helpers may impose additional mode limits.
 */
static void run(InputCfg* cfg, InPutComplete cb, void* cbData) {
    InputController* ctrl;
    if (!cfg) {
        LOG_TRACE("inputMgr: NULL configuration");
        return;
    }
    if (active || entered) {
        LOG_TRACE(
            "inputMgr: leave the current input state before starting again");
        return;
    }
    ctrl = controllerFor(cfg->type);
    if (!ctrl || !ctrl->provider || !ctrl->provider->start ||
        !ctrl->provider->stop || !ctrl->cb || !ctrl->cb->handle ||
        cfg->minLen > cfg->maxLen || cfg->maxLen >= INPUT_SIZE ||
        cfg->mode < INMD_ENTER_AMOUNT || cfg->mode >= INMD_ALL) {
        LOG_TRACE("inputMgr: invalid configuration or controller");
        return;
    }

    /* Copy first, allowing cfg to refer to this controller's existing cfg. */
    InputCfg request = *cfg;
    ctrl->cfg        = request;
    reset(request.type);
    detachRequest();
    manager.ctrl   = ctrl;
    manager.input  = ctrl->input;
    manager.finput = ctrl->finput;
    manager.cb     = cb;
    manager.cbData = cbData;
    active         = true;
    OOP_CALL(manager.state, setTimeOut, request.timeout);
    SM_GOTO(manager.state);
}

/** Lazily initialize the singleton; allocation failure remains retryable. */
InputMgr* inmgr(void) {
    if (initialized)
        return &manager;
    manager.state = MEM_ALLOC(sizeof(State));
    if (!manager.state) {
        LOG_TRACE("inputMgr: state allocation failed");
        return NULL;
    }
    OOP_CALL_CTOR(State, manager.state, NULL, "");
    manager.run                         = run;
    manager.set                         = set;
    manager.setDefault                  = set;
    manager.commit                      = commit;
    manager.reset                       = reset;
    manager.state->vtable.enter         = STATE_ENTER(Input);
    manager.state->vtable.exit          = STATE_EXIT(Input);
    manager.state->vtable.handleKeypad  = STATE_HANDLE(Input, KeypadEvent);
    manager.state->vtable.handlePed     = STATE_HANDLE(Input, PedEvent);
    manager.state->vtable.handleTimeout = STATE_HANDLE(Input, TimeOutEvent);
    inputInit(&keypadCtrl);
    inputInit(&pedCtrl);
    ui_create();
    initialized = true;
    return &manager;
}
