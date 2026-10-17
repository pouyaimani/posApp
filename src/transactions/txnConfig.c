#include "txnConfig.h"
#include "txnFlow/txnFLow.h"
#include "phrases/phrases.h"
#include "states/states.h"
#include "ui/infoPage.h"
#include "settings/settings.h"
#include "sys/sys.h"
#include "magReader/magReader.h"
#include "states.h"
#include "iso/iso8583.h"
#include "states/states.h"
#include "states/supporter.h"
#include "infoMgr/infoMgr.h"

/******************************************************************
 *                Balance Transaction config
 ******************************************************************/

static const uint8_t isoFeildsBalance[] = {ELEMENT_PAN,
                                           ELEMENT_PROCESSING_CODE,
                                           ELEMENT_STAN,
                                           ELEMENT_TIME_LOCAL_TRANSACTION,
                                           ELEMENT_DATE_LOCAL_TRANSACTION,
                                           ELEMENT_POS_ENTRY_MODE,
                                           ELEMENT_NETWORK_INTL_ID,
                                           ELEMENT_POS_CONDITION_CODE,
                                           ELEMENT_ACQUIRING_INSTITUTION_ID,
                                           ELEMENT_TRACK2,
                                           ELEMENT_TERMINAL_ID,
                                           ELEMENT_CARD_ACCEPTOR_ID,
                                           ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                           ELEMENT_PIN_DATA,
                                           ELEMENT_SECURITY_CONTROL_INFO,
                                           ELEMENT_MAC};

/******************************************************************
 *                Bill Transaction config
 ******************************************************************/

static const uint8_t isoFeildsBill[] = {ELEMENT_PAN,
                                        ELEMENT_PROCESSING_CODE,
                                        ELEMENT_AMOUNT_TRANSACTION,
                                        ELEMENT_STAN,
                                        ELEMENT_TIME_LOCAL_TRANSACTION,
                                        ELEMENT_DATE_LOCAL_TRANSACTION,
                                        ELEMENT_POS_ENTRY_MODE,
                                        ELEMENT_NETWORK_INTL_ID,
                                        ELEMENT_POS_CONDITION_CODE,
                                        ELEMENT_ACQUIRING_INSTITUTION_ID,
                                        ELEMENT_TRACK2,
                                        ELEMENT_TERMINAL_ID,
                                        ELEMENT_CARD_ACCEPTOR_ID,
                                        ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                        ELEMENT_CURRENCY_CODE_TRANSACTION,
                                        ELEMENT_PIN_DATA,
                                        ELEMENT_SECURITY_CONTROL_INFO,
                                        ELEMENT_MAC};

/******************************************************************
 *                Configuration Transaction config
 ******************************************************************/
static const uint8_t isoFeildsCfg[] = {
    ELEMENT_PROCESSING_CODE, ELEMENT_STAN, ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION, ELEMENT_NETWORK_INTL_ID,
    // ELEMENT_ACQUIRING_INSTITUTION_ID,
    ELEMENT_TERMINAL_ID, ELEMENT_ADDITIONAL_DATA_PRIVATE,
    ELEMENT_SECURITY_CONTROL_INFO, ELEMENT_MAC};

/******************************************************************
 *                Logon Transaction config
 ******************************************************************/

static const uint8_t isoFeildsLogon[] = {
    ELEMENT_PROCESSING_CODE, ELEMENT_STAN, ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION, ELEMENT_NETWORK_INTL_ID,
    // ELEMENT_ACQUIRING_INSTITUTION_ID,
    ELEMENT_ADDITIONAL_DATA_PRIVATE, ELEMENT_SECURITY_CONTROL_INFO,
    ELEMENT_MAC};

/******************************************************************
 *                Purchase Transaction config
 ******************************************************************/
static const uint8_t isoFeildsPurchase[] = {ELEMENT_PAN,
                                            ELEMENT_PROCESSING_CODE,
                                            ELEMENT_AMOUNT_TRANSACTION,
                                            ELEMENT_STAN,
                                            ELEMENT_TIME_LOCAL_TRANSACTION,
                                            ELEMENT_DATE_LOCAL_TRANSACTION,
                                            ELEMENT_POS_ENTRY_MODE,
                                            ELEMENT_NETWORK_INTL_ID,
                                            ELEMENT_POS_CONDITION_CODE,
                                            ELEMENT_ACQUIRING_INSTITUTION_ID,
                                            ELEMENT_TRACK2,
                                            ELEMENT_TERMINAL_ID,
                                            ELEMENT_CARD_ACCEPTOR_ID,
                                            ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                            ELEMENT_CURRENCY_CODE_TRANSACTION,
                                            ELEMENT_PIN_DATA,
                                            ELEMENT_SECURITY_CONTROL_INFO,
                                            ELEMENT_MAC};

/******************************************************************
 *                Reverse Transaction config
 ******************************************************************/
static const uint8_t isoFeildsReverse[] = {ELEMENT_PROCESSING_CODE,
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

/******************************************************************
 *                Settle Transaction config
 ******************************************************************/

static const uint8_t isoFeildsSettle[] = {ELEMENT_PROCESSING_CODE,
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

/******************************************************************
 *                Topup Transaction config
 ******************************************************************/
static const uint8_t isoFeildsTopUp[] = {ELEMENT_PAN,
                                         ELEMENT_PROCESSING_CODE,
                                         ELEMENT_AMOUNT_TRANSACTION,
                                         ELEMENT_STAN,
                                         ELEMENT_TIME_LOCAL_TRANSACTION,
                                         ELEMENT_DATE_LOCAL_TRANSACTION,
                                         ELEMENT_POS_ENTRY_MODE,
                                         ELEMENT_NETWORK_INTL_ID,
                                         ELEMENT_POS_CONDITION_CODE,
                                         ELEMENT_ACQUIRING_INSTITUTION_ID,
                                         ELEMENT_TRACK2,
                                         ELEMENT_TERMINAL_ID,
                                         ELEMENT_CARD_ACCEPTOR_ID,
                                         ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                         ELEMENT_CURRENCY_CODE_TRANSACTION,
                                         ELEMENT_PIN_DATA,
                                         ELEMENT_SECURITY_CONTROL_INFO,
                                         ELEMENT_MAC};

/******************************************************************
 *                Voucher Transaction config
 ******************************************************************/

static const uint8_t isoFeildsVoucher[] = {ELEMENT_PAN,
                                           ELEMENT_PROCESSING_CODE,
                                           ELEMENT_AMOUNT_TRANSACTION,
                                           ELEMENT_STAN,
                                           ELEMENT_TIME_LOCAL_TRANSACTION,
                                           ELEMENT_DATE_LOCAL_TRANSACTION,
                                           ELEMENT_POS_ENTRY_MODE,
                                           ELEMENT_NETWORK_INTL_ID,
                                           ELEMENT_POS_CONDITION_CODE,
                                           ELEMENT_ACQUIRING_INSTITUTION_ID,
                                           ELEMENT_TRACK2,
                                           ELEMENT_TERMINAL_ID,
                                           ELEMENT_CARD_ACCEPTOR_ID,
                                           ELEMENT_ADDITIONAL_DATA_PRIVATE,
                                           ELEMENT_CURRENCY_CODE_TRANSACTION,
                                           ELEMENT_PIN_DATA,
                                           ELEMENT_SECURITY_CONTROL_INFO,
                                           ELEMENT_MAC};

static TxnExtention data;
static uint64_t     selectedAmnt;

/**********************************************************************
 *
 *                     Common Transaction config
 *
 **********************************************************************/

static TxnFlowConfig* txnCfg;
static TxnFlow*       txnFlow;
static State*         txnOwnerState;
static SimCardOp_t    selectedOp;
static TxnType        txn;
static char           phoneNum[LEN_MAX_PHONE_NUMBER + 1];

static int pendMgrDone(TxnData* data) {
    LOG_TRACE("Pending manager done callback is called.");
    if (data) {
        if (data->core.respCode != RESP_CODE_SUCESS) {
            infoMgrsetCommon(STATE_IDLE, STATE_IDLE);
            infoMgr()->run(INFO_ERROR, &infoFinishCtx,
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
    // if (!txnPendingMgr()->run(pendMgrDone)) {
    //     pendMgrDone(NULL);
    // }
    return ERR_OK;
}

int composeCommon(TxnData* data) {
    magreader()->getPan(data->core.pan, sizeof(data->core.pan));
    LOG_TRACE("data->core.pan = %s", data->core.pan);
    data->dateTime = OOP_CALL(sys(), getPackedDateTime);
    return ERR_OK;
}

int buildCommon(TxnFlow* flow, ByteArray* ba) {
    flow->data->core.processCode = flow->cfg->prcode;
    flow->data->core.stan        = txnTraceInfo()->stan;
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, flow->data, ba);
}

int parseCommon(TxnFlow* flow, ByteArray* ba) {
    return isoParse(flow->cfg->mti, flow->cfg->prcode, flow->data, ba);
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
        GOTO_TXN_RES(flow->cfg, flow->data, st, onSuc, onFail);
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
    infoMgrsetCommon(onFail, onFail);
    infoMgr()->run(
        INFO_ERROR, &infoFinishCtx, phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION),
        st->result == TXN_FLOW_TIMEOUT ? phraseGetDef(PHRASE_TIME_OUT)
                                       : phraseGetDef(body));
}

void financeTxnDone(TxnFlow* flow, const TxnFlowStatus* st) {
    commonDone(flow, st, STATE_IDLE, STATE_IDLE, false);
}

#define TXN_FLOW_COMMON                                                        \
    .build = buildCommon, .parse = parseCommon,                                \
    .onConnecting = showConnecting, .onSending = showSending,                  \
    .onReceiving = showReceiving, .digitalReceipt = true

/**********************************************************************
 *
 *                     Balance Transaction config
 *
 **********************************************************************/

const TxnFlowConfig balanceTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_BALANCE,

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_BALANCE,

    .feilds = isoFeildsBalance,

    .feildsCnt = sizeof(isoFeildsBalance),

    .compose = composeCommon,

    .done = financeTxnDone,

    .needSettlement = false};

const TxnMgrCfg balanceTxnCfg = {
    .txnFlowCfg = &balanceTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Bill Transaction config
 *
 **********************************************************************/

const TxnFlowConfig billTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_BILL,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_BILL_PAYMENT,

    .feilds = isoFeildsBill,

    .feildsCnt = sizeof(isoFeildsBill),

    .compose = composeCommon,

    .done = financeTxnDone,
};

const TxnMgrCfg billTxnCfg = {
    .txnFlowCfg = &billTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Config Transaction config
 *
 **********************************************************************/
static void cfgDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        LOG_TRACE(
            "config transaction is done successfully. saving settings ...");
        settings()->save();
    }
    Supporter* state = (Supporter*)STATE_SUPPORTER;
    commonDone(flow, st, state->substate.supervisor->substate.configuration,
               state->substate.supervisor->substate.configuration, true);
}

const TxnFlowConfig cfgTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_CFG,

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_CFG,

    .feilds = isoFeildsCfg,

    .feildsCnt = sizeof(isoFeildsCfg),

    .needSettlement = false,

    .compose = NULL,

    .done = cfgDone,

    .digitalReceipt = false};

const TxnMgrCfg cfgTxnCfg = {
    .txnFlowCfg = &cfgTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Logon Transaction config
 *
 **********************************************************************/
static void logOnDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    Supporter* state = (Supporter*)STATE_SUPPORTER;
    commonDone(flow, st, state->substate.supervisor->substate.configuration,
               state->substate.supervisor->substate.configuration, true);
}

const TxnFlowConfig logOnTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_LOGON,

    .mti = MTI_NET_REQ,

    .prcode = PRC_LOG_ON,

    .feilds = isoFeildsLogon,

    .feildsCnt = sizeof(isoFeildsLogon),

    .needSettlement = false,

    .compose = NULL,

    .done = logOnDone,

    .digitalReceipt = false};

const TxnMgrCfg logonTxnCfg = {
    .txnFlowCfg = &logOnTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Purchase Transaction config
 *
 **********************************************************************/

const TxnFlowConfig purchaseTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_PURCHASE,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeildsPurchase,

    .needSettlement = true,

    .feildsCnt = sizeof(isoFeildsPurchase),

    .compose = composeCommon,

    .done = financeTxnDone,
};

const TxnMgrCfg purchaseTxnCfg = {
    .txnFlowCfg = &purchaseTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Reverse Transaction config
 *
 **********************************************************************/
extern void reverseDone(TxnFlow* flow, const TxnFlowStatus* st);

static int composeReverse(TxnData* data) {
    LOG_TRACE("composeReverse: stan before = %lu, stan after = %lu",
              data->core.stan, txnTraceInfo()->stan);
    data->core.stan = txnTraceInfo()->stan;
}

int buildReverse(TxnFlow* flow, ByteArray* ba) {
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, flow->data, ba);
}

const TxnFlowConfig reverseTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_REVERSE,

    .mti = MTI_REV_ADVICE,

    .prcode = PRC_REVERSE,

    .feilds = isoFeildsReverse,

    .feildsCnt = sizeof(isoFeildsReverse),

    .needSettlement = false,

    .compose = composeReverse,

    .build = buildReverse,

    .done = reverseDone,
};

const TxnMgrCfg reverseTxnCfg = {
    .txnFlowCfg = &reverseTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Settle Transaction config
 *
 **********************************************************************/

extern void settleDone(TxnFlow* flow, const TxnFlowStatus* st);

static int composeSettle(TxnData* data) {
    data->core.stan = txnTraceInfo()->stan;
}

int buildSettle(TxnFlow* flow, ByteArray* ba) {
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, flow->data, ba);
}

const TxnFlowConfig settlementTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_SETTLE,

    .mti = MTI_FIN_ADVICE,

    .prcode = PRC_SETTLE,

    .feilds = isoFeildsSettle,

    .feildsCnt = sizeof(isoFeildsSettle),

    .needSettlement = false,

    .compose = composeSettle,

    .build = buildSettle,

    .done = settleDone,
};

const TxnMgrCfg settlementTxnCfg = {
    .txnFlowCfg = &settlementTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};
/**********************************************************************
 *
 *                     Topup Transaction config
 *
 **********************************************************************/

const TxnFlowConfig topupTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_TOPUP,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_TOPUP,

    .feilds = isoFeildsTopUp,

    .feildsCnt = sizeof(isoFeildsTopUp),

    .needSettlement = true,

    .compose = composeCommon,

    .done = financeTxnDone};

const TxnMgrCfg topupTxnCfg = {
    .txnFlowCfg = &topupTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Voucher Transaction config
 *
 **********************************************************************/

const TxnFlowConfig voucherTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_VOUCHER,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_VOUCHER,

    .feilds = isoFeildsVoucher,

    .feildsCnt = sizeof(isoFeildsVoucher),

    .needSettlement = true,

    .compose = composeCommon,

    .done = financeTxnDone};

const TxnMgrCfg voucherTxnCfg = {
    .txnFlowCfg = &voucherTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};