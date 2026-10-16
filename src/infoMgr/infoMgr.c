
#include "infoMgr.h"
#include "logger.h"
#include "utility/utility.h"
#include "phrases/phrases.h"
#include "error.h"
#include "states/states.h"
#include "ui/devInfoPage.h"
#include "receipt/receiptTemplates.h"

static InfoMgr __infomgr;

lv_obj_t* devInfoPage;

InfoFinishCtx infoFinishCtx;

void infoDoneCommon(InfoResult result, void* userData) {
    if (result == INFO_RES_FINISH) {
        if (infoFinishCtx.onFinish)
            SM_GOTO(infoFinishCtx.onFinish);
    } else if (result == INFO_RES_CANCEL) {
        if (infoFinishCtx.onCancel)
            SM_GOTO(infoFinishCtx.onCancel);
    }
}

void infoMgrsetCommon(State* onFinish, State* onCancel) {
    memset(&infoFinishCtx, 0, sizeof(InfoFinishCtx));
    infoFinishCtx.onFinish = onFinish;
    infoFinishCtx.onCancel = onCancel;
    infoFinishCtx.done     = infoDoneCommon;
}

STATE_DEF_ENTER(InfoMgr) {}

STATE_DEF_HANDLE(InfoMgr, KeypadEvent) {
    InfoResult res;
    if (ev->key == KEY_ENTER) {
        res = INFO_RES_FINISH;
    } else if (ev->key == KEY_ESC) {
        res = INFO_RES_CANCEL;
    } else {
        return;
    }
    if (__infomgr.mode == INFO_MODE_NORMAL) {
        infoPage()->hide();
    } else if (__infomgr.mode == INFO_MODE_DEV_INFO) {
        ui_dev_info_hide(devInfoPage);
        ui_dev_info_destroy(devInfoPage);
    } else if (__infomgr.mode == INFO_MODE_DIGITAL_REC) {
        hideDigitalRec();
    }
    if (__infomgr.finishCtx.done) {
        LOG_TRACE("InfoMgr: calling done callback.");
        __infomgr.finishCtx.done(res, __infomgr.finishCtx.userData);
    }
}

STATE_DEF_HANDLE(InfoMgr, TimeOutEvent) {
    if (__infomgr.mode == INFO_MODE_NORMAL) {
        infoPage()->hide();
    } else if (__infomgr.mode == INFO_MODE_DEV_INFO) {
        ui_dev_info_hide(devInfoPage);
        ui_dev_info_destroy(devInfoPage);
    } else if (__infomgr.mode == INFO_MODE_DIGITAL_REC) {
        hideDigitalRec();
    }
    SM_GOTO(STATE_IDLE);
}

static void run(InfoType_t type, InfoFinishCtx* ctx, const char* title,
                const char* body) {
    __infomgr.finishCtx = *ctx;
    __infomgr.mode      = INFO_MODE_NORMAL;
    infoPage()->setData(type, title, body);
    SM_GOTO(__infomgr.state);
    infoPage()->show();
}

static void runDevInfo(InfoFinishCtx* ctx) {
    devInfoPage = ui_dev_info_create();
    RETURN_IF_NULL(devInfoPage, ;);
    __infomgr.finishCtx = *ctx;
    __infomgr.mode      = INFO_MODE_DEV_INFO;
    ui_dev_info_show(devInfoPage);
    SM_GOTO(__infomgr.state);
}
static void runDigitalReceipt(InfoFinishCtx* ctx, TxnData* txnData) {
    if (!ctx || !txnData) {
        LOG_ERROR("InfoMgr: missing receipt context or transaction data");
        return;
    }

    LOG_TRACE("InfoMgr: data=%p, pan buffer=%p", (void*)txnData,
              (void*)txnData->core.pan);

    __infomgr.finishCtx = *ctx;
    __infomgr.mode      = INFO_MODE_DIGITAL_REC;

    showDigitalRec(txnData);

    SM_GOTO(__infomgr.state);
}

OOP_CTOR(InfoMgr) {
    self->state = MEM_ALLOC(sizeof(State));
    OOP_CALL_CTOR(State, self->state, NULL, "");
    self->run                         = run;
    self->runDevInfo                  = runDevInfo;
    self->runDigitalReceipt           = runDigitalReceipt;
    self->state->vtable.enter         = STATE_ENTER(InfoMgr);
    self->state->vtable.handleKeypad  = STATE_HANDLE(InfoMgr, KeypadEvent);
    self->state->vtable.handleTimeout = STATE_HANDLE(InfoMgr, TimeOutEvent);
}

InfoMgr* infoMgr() {
    CALL_ONCE(OOP_CALL_CTOR(InfoMgr, &__infomgr););
    return &__infomgr;
}