#include "txnFlow/txnFLow.h"
#include "isoHandler.h"
#include "settings/settings.h"
#include "txnCommon.h"

extern void reverseDone(TxnFlow* flow, const TxnFlowStatus* st);

static int compose(TxnData* data) { data->core.stan = txnTraceInfo()->stan; }

static const uint8_t isoFeilds[] = {ELEMENT_PROCESSING_CODE,
                                    ELEMENT_AMOUNT_TRANSACTION,
                                    ELEMENT_STAN,
                                    ELEMENT_TIME_LOCAL_TRANSACTION,
                                    ELEMENT_DATE_LOCAL_TRANSACTION,
                                    ELEMENT_NETWORK_INTL_ID,
                                    ELEMENT_ACQUIRING_INSTITUTION_ID,
                                    ELEMENT_TERMINAL_ID,
                                    ELEMENT_CARD_ACCEPTOR_ID,
                                    ELEMENT_CURRENCY_CODE_TRANSACTION,
                                    ELEMENT_SECURITY_CONTROL_INFO,
                                    ELEMENT_ORIGINAL_DATA_ELEMENTS,
                                    ELEMENT_MAC_2};

int buildReverse(TxnFlow* flow, ByteArray* ba) {
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, &flow->data, ba);
}

const TxnFlowConfig reverseTxn = {

    TXN_FLOW_COMMON,

    .type = TXN_REVERSE,

    .mti = MTI_REV_ADVICE,

    .prcode = PRC_REVERSE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .needSettlement = false,

    .compose = compose,

    .build = buildReverse,

    .done = reverseDone,
};
