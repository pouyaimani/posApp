#include "txnFlow/txnFLow.h"
#include "phrases/phrases.h"
#include "states/states.h"
#include "ui/infoPage.h"
#include "settings/settings.h"
#include "sys/sys.h"
#include "magReader/magReader.h"
#include "txnFlow/txnPendingMgr.h"
#include "states.h"

static TxnFlowConfig* txnCfg;
static TxnFlow*       txnFlow;
static State*         txnOwnerState;

static int pendMgrDone(TxnData* data) {
    LOG_TRACE("Pending manager done callback is called.");
    if (data) {
        if (data->core.respCode != RESP_CODE_SUCESS) {
            GOTO_INFO(STATE_IDLE, STATE_IDLE, INFO_ERROR,
                      phraseGetDef(PHRASE_PENDED_TXN_FAILURE), "");
            return;
        }
    }
    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(txnFlow, txnOwnerState, ip, settings()->server.mainServerPort,
           txnCfg);
}

int8_t txnStart(TxnFlowConfig* cfg, TxnFlow* flow, State* owner) {
    txnCfg        = cfg;
    txnFlow       = flow;
    txnOwnerState = owner;
    if (!txnPendingMgr()->run(pendMgrDone)) {
        pendMgrDone(NULL);
    }
    return ERR_OK;
}

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
    LOG_TRACE("transaction result = %d", st->result);
    if (st->result == TXN_FLOW_SUCCESS) {
        GOTO_TXN_RES(flow->cfg, &flow->data, st, onSuc, onFail);
        return;
    }

    Phrases_t body = PHRASE_CONNECTION_ERR;
    LOG_TRACE("transaction stage = %d", st->stage);
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

void financeTxnDone(TxnFlow* flow, const TxnFlowStatus* st) {
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
}