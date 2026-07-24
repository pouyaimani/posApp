#include "txnOrchestrator/txnFLow.h"
#include "phrases/phrases.h"
#include "states/states.h"
#include "ui/infoPage.h"
#include "settings/settings.h"
#include "sys/sys.h"
#include "magReader/magReader.h"

int composeCommon(TxnData* data) {
    magreader()->getPan(data->core.pan, sizeof(data->core.pan));
    // DateTime* dt = OOP_CALL(sys(), getDateTime);
    // iso8583()->setStr(ELEMENT_TIME_LOCAL_TRANSACTION,
    //                   (const DL_UINT8*)dt->time);
    // LOG_TRACE("| Bit 12 - (Time) | -> %s", dt->time);
    // data->dateTime = OOP_CALL(sys(), getPackedDateTime);
    return ERR_OK;
}

int buildCommon(TxnFlow* flow, ByteArray* ba) {
    flow->data.core.processCode = flow->cfg->prcode;
    flow->data.core.stan        = txnTraceInfo()->stan;
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, &flow->data, ba);
}

int parseCommon(TxnFlow* flow, ByteArray* ba) {
    return isoParse(flow->cfg->mti, flow->cfg->prcode, &flow->data, ba);
}

void showConnecting(TxnFlow* f) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CONNECTIING), "");
}

void showSending(TxnFlow* f) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_SENDING_DATA), "");
}

void showReceiving(TxnFlow* f) {
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_RECEIVING_DATA), "");
}

void commonDone(TxnFlow* flow, const TxnFlowStatus* st, State* onSuc,
                State* onFail, bool showSucMsg) {
    State* state = flow->owner;
    LOG_DEBUG("st->result = %d", st->result);
    if (st->result == TXN_FLOW_SUCCESS) {
        if (!showSucMsg) {
            HIDE_INFO();
            return;
        }
        DEFINE_STRING(dsc, 128);
        if (st->code != 0) {
            getResponseCode(st->code, dsc, sizeof(dsc));
        }
        if (st->code == 0) {
            GOTO_INFO(onSuc, onSuc, INFO_SUCCESS,
                      phraseGetDef(PHRASE_SUC_DONME), dsc);
        } else {
            GOTO_INFO(onFail, onFail, INFO_ERROR,
                      phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION), dsc);
        }
        GOTO_INFO(st->code == 0 ? onSuc : onFail,
                  st->code == 0 ? onSuc : onFail,
                  st->code == 0 ? INFO_SUCCESS : INFO_ERROR,
                  phraseGetDef(st->code == 0 ? PHRASE_SUC_DONME
                                             : PHRASE_UNSUCCESSFUL_OPERATION),
                  dsc);
        return;
    }

    Phrases_t body = PHRASE_CONNECTION_ERR;

    switch (st->stage) {
    case TXN_STAGE_SENDING:
        body = PHRASE_SENDING_DATA_ERR;
        break;

    case TXN_STAGE_RECEIVING:
        body = PHRASE_RECEIVING_DATA_ERR;
        break;
    case TXN_STAGE_PARSING:
        body = PHRASE_PARSE_ERROR;
        break;

    default:
        break;
    }
    GOTO_INFO(onFail, onFail, INFO_ERROR,
              phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION),
              st->result == TXN_FLOW_TIMEOUT ? phraseGetDef(PHRASE_TIME_OUT)
                                             : phraseGetDef(body));
}