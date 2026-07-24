#include "txnOrchestrator/txnFLow.h"
#include "isoBuilder.h"
#include "settings/settings.h"
#include "states/states.h"

extern void settleDone(TxnFlow* flow, const TxnFlowStatus* st);

static int compose(TxnData* data) { data->core.stan = txnTraceInfo()->stan; }

static const uint8_t isoFeilds[] = {ELEMENT_PROCESSING_CODE,
                                    ELEMENT_AMOUNT_TRANSACTION,
                                    ELEMENT_STAN,
                                    ELEMENT_TIME_LOCAL_TRANSACTION,
                                    ELEMENT_DATE_LOCAL_TRANSACTION,
                                    ELEMENT_NETWORK_INTL_ID,
                                    ELEMENT_ACQUIRING_INSTITUTION_ID,
                                    ELEMENT_RETRIEVAL_REFERENCE_NUMBER,
                                    ELEMENT_TERMINAL_ID,
                                    ELEMENT_CARD_ACCEPTOR_ID,
                                    ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                    ELEMENT_CURRENCY_CODE_TRANSACTION,
                                    ELEMENT_SECURITY_CONTROL_INFO,
                                    ELEMENT_MAC};

int buildSettle(TxnFlow* flow, ByteArray* ba) {
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, &flow->data, ba);
}

const TxnFlowConfig settlementTxn = {

    .mti = MTI_FIN_ADVICE,

    .prcode = PRC_SETTLE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .needSettlement = false,

    .compose = compose,

    .build = buildSettle,

    .parse = parseCommon,

    .done = settleDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};
