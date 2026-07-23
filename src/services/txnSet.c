#include "txnOrchestrator/txnFLow.h"
#include "isoBuilder.h"

static int compose(TxnData* data) {}

static void settleDone(TxnFlow* flow, const TxnFlowStatus* st) {
    // commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
}

static const uint8_t isoFeilds[] = {ELEMENT_PAN,
                                    ELEMENT_PROCESSING_CODE,
                                    ELEMENT_AMOUNT_TRANSACTION,
                                    ELEMENT_STAN,
                                    ELEMENT_TIME_LOCAL_TRANSACTION,
                                    ELEMENT_DATE_LOCAL_TRANSACTION,
                                    ELEMENT_POS_ENTRY_MODE,
                                    ELEMENT_NETWORK_INTL_ID,
                                    ELEMENT_ACQUIRING_INSTITUTION_ID,
                                    ELEMENT_RETRIEVAL_REFERENCE_NUMBER,
                                    ELEMENT_TERMINAL_ID,
                                    ELEMENT_CARD_ACCEPTOR_ID,
                                    ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                    ELEMENT_CURRENCY_CODE_TRANSACTION,
                                    ELEMENT_SECURITY_CONTROL_INFO,
                                    ELEMENT_MAC};

const TxnFlowConfig settlementTxn = {

    .mti = MTI_FIN_ADVICE,

    .prcode = PRC_SETTLE,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .compose = compose,

    .build = buildCommon,

    .parse = parseCommon,

    .done = settleDone,

    .onConnecting = showConnecting,

    .onSending = showSending,

    .onReceiving = showReceiving};
