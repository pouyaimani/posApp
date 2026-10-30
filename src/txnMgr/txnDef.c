#include "txnDef.h"
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
#include "common.h"

/******************************************************************
 *                      Transaction definitions
 ******************************************************************/

/*
 * Each array defines the logical execution order for one transaction.
 *
 * A step reports success through next() or terminal failure through
 * fail(code). Internal steps do not require separate State objects.
 */
const TxnMgrStep purchaseSteps[] = {
    TXN_STEP_MAG,     TXN_STEP_AMOUNT,        TXN_STEP_PIN,
    TXN_STEP_PENDING, TXN_STEP_COMMUNICATION, TXN_STEP_SETTLEMENT};

const TxnMgrStep balanceSteps[] = {TXN_STEP_MAG, TXN_STEP_PIN,
                                   TXN_STEP_COMMUNICATION};

const TxnMgrStep billSteps[] = {
    TXN_STEP_MAG,     TXN_STEP_BILL_ID,       TXN_STEP_PAYMENT_ID, TXN_STEP_PIN,
    TXN_STEP_PENDING, TXN_STEP_COMMUNICATION, TXN_STEP_SETTLEMENT};

const TxnMgrStep voucherSteps[] = {
    TXN_STEP_MAG,       TXN_STEP_OPERATOR, TXN_STEP_CHARGE_AMOUNT,
    TXN_STEP_PIN,       TXN_STEP_PENDING,  TXN_STEP_COMMUNICATION,
    TXN_STEP_SETTLEMENT};

const TxnMgrStep topupSteps[] = {TXN_STEP_MAG,           TXN_STEP_OPERATOR,
                                 TXN_STEP_CHARGE_AMOUNT, TXN_STEP_PHONE,
                                 TXN_STEP_PIN,           TXN_STEP_PENDING,
                                 TXN_STEP_COMMUNICATION, TXN_STEP_SETTLEMENT};

const TxnMgrStep configSteps[] = {TXN_STEP_COMMUNICATION};
const TxnMgrStep logonSteps[]  = {TXN_STEP_COMMUNICATION};

const TxnMgrStep kahrobaPurchaseSteps[] = {
    TXN_STEP_AMOUNT,        TXN_STEP_NFC,      TXN_STEP_PENDING,
    TXN_STEP_COMMUNICATION, TXN_STEP_HOST_PIN, TXN_STEP_COMMUNICATION_PIN,
    TXN_STEP_SETTLEMENT};

/**
 * @brief Kahroba balance inquiry transaction sequence.
 *
 * NFC:
 *   Read card data and generate ARQC with zero amount
 *   and EMV transaction type 0x31.
 *
 * PIN:
 *   Obtain the encrypted online PIN block.
 *
 * COMMUNICATION:
 *   Send the balance inquiry request to the host.
 *
 * Balance inquiry does not require settlement.
 */
static const TxnMgrStep kahrobaBalanceSteps[] = {TXN_STEP_NFC, TXN_STEP_PIN,
                                                 TXN_STEP_COMMUNICATION};

#define TXN_DEFINITION(type_, steps_) {(type_), (steps_), ARRAY_COUNT(steps_)}

static const TxnDefinition definitions[] = {
    TXN_DEFINITION(TXN_PURCHASE, purchaseSteps),
    TXN_DEFINITION(TXN_BALANCE, balanceSteps),
    TXN_DEFINITION(TXN_BILL, billSteps),
    TXN_DEFINITION(TXN_VOUCHER, voucherSteps),
    TXN_DEFINITION(TXN_TOPUP, topupSteps),
    TXN_DEFINITION(TXN_CFG, configSteps),
    TXN_DEFINITION(TXN_LOGON, logonSteps),
    TXN_DEFINITION(TXN_KAHROBA_PURCHASE, kahrobaPurchaseSteps),
    TXN_DEFINITION(TXN_KAHROBA_BALANCE, kahrobaBalanceSteps),
};

/*
 * Return the definition for a supported transaction.
 * The returned definition and its step array have static lifetime.
 */
const TxnDefinition* findDefinition(TxnType type) {
    for (size_t i = 0; i < ARRAY_COUNT(definitions); ++i) {
        if (definitions[i].type == type) {
            return &definitions[i];
        }
    }

    return NULL;
}

/**********************************************************************
 *
 *                     Common Transaction config
 *
 **********************************************************************/

int composeCommon(TxnFlow* flow) {
    TxnData* data  = flow->data;
    data->dateTime = OOP_CALL(sys(), getPackedDateTime);
    return ERR_OK;
}

int buildCommon(TxnFlow* flow, ByteArray* ba) {
    flow->data->core.processCode = flow->cfg->prcode;
    flow->data->core.stan        = txnTraceInfo()->stan;
    return isoBuild(flow->cfg->mti, flow->cfg->prcode, flow->cfg->feilds,
                    flow->cfg->feildsCnt, flow->data, ba, flow->userData);
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

#define TXN_FLOW_COMMON                                                        \
    .build = buildCommon, .parse = parseCommon,                                \
    .onConnecting = showConnecting, .onSending = showSending,                  \
    .onReceiving = showReceiving, .digitalReceipt = true

/**********************************************************************
 *
 *                          Balance Transaction
 *
 **********************************************************************/

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

const TxnFlowConfig balanceTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_BALANCE,

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_BALANCE,

    .feilds = isoFeildsBalance,

    .feildsCnt = sizeof(isoFeildsBalance),

    .compose = composeCommon,

    .needSettlement = false};

const TxnMgrCfg balanceTxnCfg = {
    .txnFlowCfg = &balanceTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Bill Transaction config
 *
 **********************************************************************/

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

const TxnFlowConfig billTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_BILL,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_BILL_PAYMENT,

    .feilds = isoFeildsBill,

    .feildsCnt = sizeof(isoFeildsBill),

    .compose = composeCommon,
};

const TxnMgrCfg billTxnCfg = {
    .txnFlowCfg = &billTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Config Transaction config
 *
 **********************************************************************/

static const uint8_t isoFeildsCfg[] = {
    ELEMENT_PROCESSING_CODE, ELEMENT_STAN, ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION, ELEMENT_NETWORK_INTL_ID,
    // ELEMENT_ACQUIRING_INSTITUTION_ID,
    ELEMENT_TERMINAL_ID, ELEMENT_ADDITIONAL_DATA_PRIVATE,
    ELEMENT_SECURITY_CONTROL_INFO, ELEMENT_MAC};

const TxnFlowConfig cfgTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_CFG,

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_CFG,

    .feilds = isoFeildsCfg,

    .feildsCnt = sizeof(isoFeildsCfg),

    .needSettlement = false,

    .compose = NULL,

    .digitalReceipt = false};

const TxnMgrCfg cfgTxnCfg = {
    .txnFlowCfg = &cfgTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Logon Transaction config
 *
 **********************************************************************/

static const uint8_t isoFeildsLogon[] = {
    ELEMENT_PROCESSING_CODE, ELEMENT_STAN, ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION, ELEMENT_NETWORK_INTL_ID,
    // ELEMENT_ACQUIRING_INSTITUTION_ID,
    ELEMENT_ADDITIONAL_DATA_PRIVATE, ELEMENT_SECURITY_CONTROL_INFO,
    ELEMENT_MAC};

const TxnFlowConfig logOnTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_LOGON,

    .mti = MTI_NET_REQ,

    .prcode = PRC_LOG_ON,

    .feilds = isoFeildsLogon,

    .feildsCnt = sizeof(isoFeildsLogon),

    .needSettlement = false,

    .compose = NULL,

    .digitalReceipt = false};

const TxnMgrCfg logonTxnCfg = {
    .txnFlowCfg = &logOnTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Purchase Transaction config
 *
 **********************************************************************/

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

const TxnFlowConfig purchaseTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_PURCHASE,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeildsPurchase,

    .needSettlement = true,

    .feildsCnt = sizeof(isoFeildsPurchase),

    .compose = composeCommon,
};

const TxnMgrCfg purchaseTxnCfg = {
    .txnFlowCfg = &purchaseTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Reverse Transaction config
 *
 **********************************************************************/

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

    .build = buildReverse,
};

const TxnMgrCfg reverseTxnCfg = {
    .txnFlowCfg = &reverseTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Settle Transaction config
 *
 **********************************************************************/

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

    .build = buildSettle,
};

const TxnMgrCfg settlementTxnCfg = {
    .txnFlowCfg = &settlementTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};
/**********************************************************************
 *
 *                     Topup Transaction config
 *
 **********************************************************************/

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

const TxnFlowConfig topupTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_TOPUP,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_TOPUP,

    .feilds = isoFeildsTopUp,

    .feildsCnt = sizeof(isoFeildsTopUp),

    .needSettlement = true,

    .compose = composeCommon};

const TxnMgrCfg topupTxnCfg = {
    .txnFlowCfg = &topupTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Voucher Transaction config
 *
 **********************************************************************/

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

const TxnFlowConfig voucherTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_VOUCHER,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_VOUCHER,

    .feilds = isoFeildsVoucher,

    .feildsCnt = sizeof(isoFeildsVoucher),

    .needSettlement = true,

    .compose = composeCommon,
};

const TxnMgrCfg voucherTxnCfg = {
    .txnFlowCfg = &voucherTxnFlowCfg, .onSuccess = NULL, .onFailure = NULL};

/**********************************************************************
 *
 *                     Kahroba Purchase Transaction config
 *
 **********************************************************************/

static const uint8_t isoFeildsKahrobaPurchase[] = {
    ELEMENT_PAN,
    ELEMENT_PROCESSING_CODE,
    ELEMENT_AMOUNT_TRANSACTION,
    ELEMENT_STAN,
    ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION,
    ELEMENT_POS_ENTRY_MODE,
    ELEMENT_CARD_SEQUENCE_NUMBER,
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
    ELEMENT_ICC_DATA,
    ELEMENT_RESERVED_NATIONAL_116,
    ELEMENT_MAC_2,
};

/**
 * @brief Prepare Kahroba transaction for online authorization.
 *
 * Called after NFC processing and before ISO 8583 building.
 *
 * PAN, amount, and EMV Field 55 must already be available.
 */
static int composeKahroba(TxnFlow* flow) {
    if (!flow || !flow->data || !flow->userData) {
        LOG_TRACE("Kahroba: compose invalid flow or session");
        return ERR_NOK;
    }

    TxnData* txn = flow->data;

    KahrobaTxnSession* session = (KahrobaTxnSession*)flow->userData;

    if (txn->core.txnType != TXN_KAHROBA_PURCHASE &&
        txn->core.txnType != TXN_KAHROBA_BALANCE) {
        LOG_TRACE("Kahroba: compose invalid transaction type");
        return ERR_NOK;
    }

    /* PAN was populated by EMV, not the magnetic reader. */
    if (!memchr(txn->core.pan, '\0', sizeof(txn->core.pan)) ||
        txn->core.pan[0] == '\0') {
        LOG_TRACE("Kahroba: compose PAN unavailable or unterminated");
        return ERR_NOK;
    }

    if (txn->core.amount != session->emv.authorizedAmount) {
        LOG_TRACE("Kahroba: compose invalid authorization amount");
        return ERR_NOK;
    }

    if (!session->emvReady || session->field55Len == 0u ||
        session->field55Len > sizeof(session->field55)) {

        LOG_TRACE("Kahroba: compose invalid Field55");
        return ERR_NOK;
    }

    /* Prepare transaction metadata without changing EMV values. */
    txn->dateTime = OOP_CALL(sys(), getPackedDateTime);

    LOG_TRACE("Kahroba: compose successful, Field55 length=%lu",
              (unsigned long)session->field55Len);

    return ERR_OK;
}

const TxnFlowConfig kahrobaPurchaseTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_KAHROBA_PURCHASE,

    .mti = MTI_FIN_REQ,

    .prcode = PRC_PURCHASE,

    .feilds = isoFeildsKahrobaPurchase,

    .feildsCnt = sizeof(isoFeildsKahrobaPurchase),

    .compose = composeKahroba,
};

/**
 * @brief Resolve host responses for Kahroba purchase transactions.
 *
 * Rules:
 * - Approved response: continue the transaction.
 * - Response 76 on first authorization: request online PIN.
 * - Response 76 on subsequent authorization: decline.
 * - Other responses: decline.
 *
 * This callback only selects an action.
 * It must not change transaction state or perform I/O.
 */
TxnHostAction kahrobaResponsePolicy(TxnMgrStep step, int responseCode,
                                    uint8_t attempt) {
    if (responseCode == RESP_CODE_SUCESS) {
        return TXN_HOST_APPROVE;
    }

    if (responseCode == KAHROBA_RESP_PIN_REQUIRED &&
        step == TXN_STEP_COMMUNICATION && attempt == 0u) {

        return TXN_HOST_REQUIRE_PIN;
    }

    return TXN_HOST_DECLINE;
}

const TxnMgrCfg kahrobaPurchaseTxnCfg = {
    .txnFlowCfg     = &kahrobaPurchaseTxnFlowCfg,
    .onSuccess      = NULL,
    .onFailure      = NULL,
    .responsePolicy = kahrobaResponsePolicy};

/**********************************************************************
 *
 *                     Kahroba Balance Transaction config
 *
 **********************************************************************/

static const uint8_t isoFeildsKahrobaBalance[] = {
    ELEMENT_PAN,
    ELEMENT_PROCESSING_CODE,
    ELEMENT_STAN,
    ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION,
    ELEMENT_POS_ENTRY_MODE,
    ELEMENT_CARD_SEQUENCE_NUMBER,
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
    ELEMENT_ICC_DATA,
    ELEMENT_RESERVED_NATIONAL_116,
    ELEMENT_MAC_2};

const TxnFlowConfig kahrobaBalanceTxnFlowCfg = {

    TXN_FLOW_COMMON,

    .type = TXN_KAHROBA_BALANCE,

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_BALANCE,

    .feilds = isoFeildsKahrobaBalance,

    .feildsCnt = sizeof(isoFeildsKahrobaBalance),

    .compose = composeKahroba,
};

const TxnMgrCfg kahrobaBalanceTxnCfg = {.txnFlowCfg = &kahrobaBalanceTxnFlowCfg,
                                        .onSuccess  = NULL,
                                        .onFailure  = NULL};