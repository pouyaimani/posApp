#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "display.h"
#include "event.h"
#include "eventloop.h"
#include "receipt/receiptTemplates.h"
#include "txnFlow/txnPendingMgr.h"
#include "record/txnRecs.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"

/******************** txn result state **********************/

static int pendMgrDone(const TxnData* data) {
    if (data->status == TXN_STATUS_APPROVED) {
        LOG_TRACE("Txn pending manager: transaction is approved.");
        // Result_t res = txnrecord()->insert(data);
        // if (res.err != ERR_DSC_OK) {
        //     LOG_ERROR("Inserting transaction to database is failed. error =
        //     %d "
        //               "detail code = %d",
        //               res.err, res.detail.db);
        // } else {
        //     LOG_TRACE(
        //         "Inserting transaction to database is successfully done.");
        // }
    } else if (data->status == TXN_STATUS_REVERSED) {
        LOG_TRACE("Txn pending manager: transaction is reversed.");
    } else {
        LOG_TRACE(
            "Pending manager is failed to determine transaction status (%d)",
            data->status);
    }
    SM_GOTO(STATE_IDLE);
}

STATE_DEF_ENTER(TxnResult) {
    TxnResult* self = (TxnResult*)state;
    if (self->txnCfg->digitalReceipt) {
        showDigitalRec(&self->data);
    } else {
        DEFINE_STRING(dsc, 128);
        if (self->st->code != 0) {
            getResponseCode(self->st->code, dsc, sizeof(dsc));
        }
        if (self->st->code == 0) {
            OOP_CALL(infoPage(), setData, INFO_SUCCESS,
                     phraseGetDef(PHRASE_SUC_DONME), dsc);
            OOP_CALL(infoPage(), show);
            return;
            // GOTO_INFO(self->onSuccess, self->onSuccess, INFO_SUCCESS,
            //           phraseGetDef(PHRASE_SUC_DONME), dsc);
        } else {
            OOP_CALL(infoPage(), setData, INFO_ERROR,
                     phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION), dsc);
            OOP_CALL(infoPage(), show);
            return;
            // GOTO_INFO(self->onFailure, self->onFailure, INFO_ERROR,
            //           phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION), dsc);
        }
        OOP_CALL(infoPage(), setData, INFO_ERROR,
                 phraseGetDef(self->st->code == 0
                                  ? PHRASE_SUC_DONME
                                  : PHRASE_UNSUCCESSFUL_OPERATION),
                 dsc);
        OOP_CALL(infoPage(), show);
        // GOTO_INFO(self->st->code == 0 ? self->onSuccess : self->onFailure,
        //           self->st->code == 0 ? self->onSuccess : self->onFailure,
        //           self->st->code == 0 ? INFO_SUCCESS : INFO_ERROR,
        //           phraseGetDef(self->st->code == 0
        //                            ? PHRASE_SUC_DONME
        //                            : PHRASE_UNSUCCESSFUL_OPERATION),
        //           dsc);
    }
}

STATE_DEF_EXIT(TxnResult) { OOP_CALL(infoPage(), hide); }

static int8_t printResult(TxnData* txn) {
    ReceiptData data;
    data.txn           = *txn;
    data.type          = DOC_TXN;
    data.headerApplied = true;
    Receipt  rec;
    Result_t res = buildReceipt(&rec, &data);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    res = OOP_CALL(&rec, flush);
    RETURN_VALUE_IF_NOT(res.err, ERR_DSC_OK, ;, false);
    OOP_CALL(&rec, destroy);
    return true;
}

STATE_DEF_HANDLE(TxnResult, KeypadEvent) {
    if (ev->key != KEY_ENTER && ev->key != KEY_ESC) {
        return;
    }
    TxnResult* self = (TxnResult*)state;
    hideDigitalRec();
    if (self->txnCfg->needSettlement) {
        if (txnPendingMgr()->run(pendMgrDone)) {
            if (ev->key == KEY_ENTER) {
                printResult(&self->data);
            }
            return;
        }
    }
    if (ev->key == KEY_ESC) {
        SM_GOTO(self->onFailure);
        return;
    }

    printResult(&self->data);
    SM_GOTO(self->onSuccess);
}

STATE_DEF_HANDLE(TxnResult, TimeOutEvent) {}

OOP_CTOR(TxnResult, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(TxnResult);
    self->base.vtable.exit          = STATE_EXIT(TxnResult);
    self->base.vtable.handleTimeout = STATE_HANDLE(TxnResult, TimeOutEvent);
    self->base.vtable.handleKeypad  = STATE_HANDLE(TxnResult, KeypadEvent);
}