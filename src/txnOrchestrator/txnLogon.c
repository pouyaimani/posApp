#include "txnFLow.h"
#include "states/states.h"
#include "ui/infoPage.h"
#include "txn.h"
#include "byteArray.h"
#include "phrases/phrases.h"

static int buildLogOn(
    ByteArray *ba)
{
    return isoBuild(
        MTI_LOG_ON,
        ba);
}

static int parseLogOn(
    ByteArray *ba)
{
    return isoParse(
        MTI_LOG_ON,
        ba);
}

static void logOnDone(
    TxnFlow *flow,
    const TxnFlowStatus *st) {
    State *state =
        flow->owner;
    LOG_DEBUG("st->result = %d", st->result);
    if (st->result == TXN_FLOW_SUCCESS) {
        DEFINE_STRING(dsc, 128);
        if (st->code != 0) {
            getResponseCode(st->code, dsc, sizeof(dsc));
        }
        GOTO_INFO(state->parent, state->parent,
            st->code == 0 ? INFO_SUCCESS : INFO_ERROR,
            phraseGetDef(
                st->code == 0 ? PHRASE_SUC_DONME : PHRASE_UNSUCCESSFUL_OPERATION),
            dsc);
        return;
    }

    Phrases_t title =
        PHRASE_CONNECTION_ERR;

    switch (st->stage) {
        case TXN_STAGE_SENDING:
            title = PHRASE_SENDING_DATA_ERR;
            break;

        case TXN_STAGE_RECEIVING:
        case TXN_STAGE_PARSING:
            title =PHRASE_RECEIVING_DATA_ERR;
            break;

        default:
            break;
    }
LOG_DEBUG("-------------------------------");
    GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                phraseGetDef(title),
                    st->result == TXN_FLOW_TIMEOUT ?
        phraseGetDef(PHRASE_TIME_OUT)
        :
        phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION));
}

const TxnFlowConfig logOnTxn = {

    .build =
        buildLogOn,

    .parse =
        parseLogOn,

    .done =
        logOnDone,

    .onConnecting =
        showConnecting,

    .onSending =
        showSending,

    .onReceiving =
        showReceiving
};