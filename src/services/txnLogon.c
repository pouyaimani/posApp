#include "txnOrchestrator/txnFLow.h"
#include "settings/settings.h"
#include "sys/sys.h"
#include "iso/iso8583.h"

static void logOnDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    commonDone(flow, st, flow->owner->parent, flow->owner->parent, true);
}

static const uint8_t isoFeilds[] = {
    ELEMENT_PROCESSING_CODE, ELEMENT_STAN, ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION, ELEMENT_NETWORK_INTL_ID,
    // ELEMENT_ACQUIRING_INSTITUTION_ID,
    ELEMENT_ADDITIONAL_DATA_PRIVATE, ELEMENT_SECURITY_CONTROL_INFO,
    ELEMENT_MAC};

const TxnFlowConfig logOnTxn = {

    .mti = MTI_NET_REQ,

    .prcode = PRC_LOG_ON,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .build = buildCommon,

    .parse = parseCommon,

    .done = logOnDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};