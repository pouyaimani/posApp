/*
 * Simplified Kahroba binding, intended to replace the earlier worker/event
 * bridge. Integrate into txnInputHandler.c after NFC detection.
 *
 * CRITICAL: onNfcComplete() must execute on a dedicated transaction task or
 * another thread permitted to block for the entire APDU sequence. If nfcMgr
 * calls back on the LVGL event loop, DO NOT execute this code there; schedule
 * a single synchronous transaction-task job instead.
 *
 * Application hooks txnEmvBuildTerminal/txnEmvRandom/txnEmvImportResult and
 * txnEmvFailTransaction were defined by the previous integration. The
 * supplied TxnMgr/NfcMgr types must be included from the real application.
 */
#include "txnMgr.h"
#include "nfcMgr/nfcMgr.h"
#include "emvMgr/emvMgr.h"
#include "emvMgr/emv_kahroba.h"
#include "emvMgr/emv_picc_transport.h"
#include "emvMgr/kahroba_field55.h"
#include "logger.h"
#include <string.h>
#include "emvMgr/emv_kahroba.h"
#include "error.h"
#include "phrases/phrases.h"

static NfcCfg nfcCfg;

static KahrobaTxnSession kahrobaSession;

/* EMV 9F02: 12 decimal digits. */
#define KAHROBA_EMV_MAX_AMOUNT UINT64_C(999999999999)

typedef struct {
    bool configured;

    /*
     * Values must use the same amount unit as EMV 9F02.
     * Populate from verified scheme/acquirer configuration.
     */
    uint64_t minAmount;
    uint64_t maxAmount;

} KahrobaAmountPolicy;

typedef enum {
    KAHROBA_AMOUNT_OK = 0,
    KAHROBA_AMOUNT_INVALID,
    KAHROBA_AMOUNT_TOO_SMALL,
    KAHROBA_AMOUNT_TOO_LARGE,
    KAHROBA_AMOUNT_POLICY_MISSING,
    KAHROBA_AMOUNT_POLICY_INVALID
} KahrobaAmountResult;

/**
 * Validate an amount before NFC starts.
 *
 * amountDigits: decimal ASCII amount, 1-12 digits.
 * policy: confirmed scheme/acquirer limits.
 * requirePolicy: true to reject missing policy configuration.
 */

KahrobaAmountResult kahrobaValidateAmount(const char* amountDigits,
                                          const KahrobaAmountPolicy* policy,
                                          bool      requirePolicy,
                                          uint64_t* parsedAmount) {
    uint64_t     amount = 0u;
    unsigned int digits = 0u;

    if (parsedAmount)
        *parsedAmount = 0u;

    if (!amountDigits || !parsedAmount)
        return KAHROBA_AMOUNT_INVALID;

    /*
     * Validate characters and length without relying on
     * strlen() or numeric conversions that may overflow.
     */
    for (digits = 0u; digits < 12u; ++digits) {
        char c = amountDigits[digits];

        if (c == '\0')
            break;

        if (c < '0' || c > '9')
            return KAHROBA_AMOUNT_INVALID;

        amount = amount * 10u + (uint64_t)(c - '0');
    }

    /* Reject empty or longer-than-12-digit amounts. */
    if (digits == 0u)
        return KAHROBA_AMOUNT_INVALID;

    if (digits == 12u && amountDigits[12] != '\0')
        return KAHROBA_AMOUNT_INVALID;

    if (amount == 0u)
        return KAHROBA_AMOUNT_INVALID;

    /*
     * Technical maximum for 9F02.
     * This does not represent a Kahroba business limit.
     */
    if (amount > KAHROBA_EMV_MAX_AMOUNT)
        return KAHROBA_AMOUNT_TOO_LARGE;

    /*
     * No scheme policy available yet.
     *
     * Production must require confirmed policy limits.
     * Compatibility testing may explicitly disable this
     * requirement while the scheme limits are being verified.
     */
    if (!policy || !policy->configured) {
        if (requirePolicy)
            return KAHROBA_AMOUNT_POLICY_MISSING;

        *parsedAmount = amount;
        return KAHROBA_AMOUNT_OK;
    }

    if (policy->minAmount == 0u || policy->minAmount > policy->maxAmount ||
        policy->maxAmount > KAHROBA_EMV_MAX_AMOUNT)
        return KAHROBA_AMOUNT_POLICY_INVALID;

    if (amount < policy->minAmount)
        return KAHROBA_AMOUNT_TOO_SMALL;

    if (amount > policy->maxAmount)
        return KAHROBA_AMOUNT_TOO_LARGE;

    *parsedAmount = amount;
    return KAHROBA_AMOUNT_OK;
}

void nfcInfoDone(InfoResult res, void* arg) {
    txnMgr()->fail(TXN_MGR_ERR_INPUT);
}

// #include "kahroba_amount.h"

// /* Example: load this from verified acquirer settings. */
// KahrobaAmountPolicy policy = {
//     .configured = false,
//     .minAmount = 0u,
//     .maxAmount = 0u
// };

// uint64_t validatedAmount = 0u;

// /*
//  * TODO(CONFIGURATION):
//  * Populate policy.minAmount and policy.maxAmount
//  * from confirmed Kahroba/acquirer limits, then set
//  * policy.configured = true.
//  *
//  * Keep requirePolicy=true for production.
//  */
// bool requirePolicy = true;

// KahrobaAmountResult amountResult = kahrobaValidateAmount(
//     amntStr,
//     &policy,
//     requirePolicy,
//     &validatedAmount
// );

// if (amountResult != KAHROBA_AMOUNT_OK) {
//     LOG_TRACE(
//         "Kahroba: amount validation rejected reason=%d",
//         (int)amountResult
//     );

//     /*
//      * Report an appropriate validation error using your
//      * existing transaction-handler error mechanism.
//      *
//      * Do not start nfcMgr.
//      */
//     return; /* For a void handler; adapt to its actual return type. */
// }

// LOG_TRACE(
//     "Kahroba: amount validated successfully"
// );

// /* Proceed to NFC only after validation succeeds. */
// /* nfcMgr()->run(...); */

static void handleABcardType(const NfcStatus* status, void* userData) {
    TxnMgr* txnMgr = userData;
    // TODO: validate kahroba amount
    EmvMgrError err =
        startKahrobaEmv(picc(), &txnMgr->data, &kahrobaSession.emv);
    // No need to nfc anymore
    OOP_CALL(picc(), close);

    if (err != EMV_MGR_OK) {
        kahrobaSession.emvReady = false;

        LOG_TRACE("Kahroba: EMV processing failed error=%d", (int)err);

        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_CARD_TYPE_ERROR), "");

        /* Abort this path and release the reader via its owner. */
        return;
    }

    /* Field 55 has not yet been prepared. */
    kahrobaSession.emvReady   = false;
    kahrobaSession.field55Len = 0u;

    if (!kahrobaBuildField55(
            &txnMgr->data, &kahrobaSession.emv, kahrobaSession.field55,
            sizeof(kahrobaSession.field55), &kahrobaSession.field55Len)) {

        LOG_TRACE("Kahroba: Field55 build failed");

        /* Use the existing failure path and reader cleanup. */
        return;
    }

    /* All data needed for online authorization is prepared. */
    kahrobaSession.emvReady = true;

    txnMgr->userData = &kahrobaSession;

    LOG_TRACE("Kahroba: authorization data ready "
              "Field55 length=%lu",
              (unsigned long)kahrobaSession.field55Len);

    txnMgr->next();
}

static void onNfcComplete(const NfcStatus* status, void* userData) {
    RETURN_IF_NULL(status, ;);

    if (status->result != NFC_RES_DETECTED) {
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_DETECT_ERROR), "");
        LOG_TRACE("NFC detection error. result = %d", status->result);
        OOP_CALL(picc(), close);
        return;
    }

    if (!status->readerRetained) {
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_ERROR), "");
        LOG_TRACE("Kahroba: NFC reader was not retained");
        OOP_CALL(picc(), close);
        return;
    }

    switch (status->card.cardType) {
    case PICC_CARD_A:
    case PICC_CARD_B:
        LOG_TRACE("Kahroba: compatible card candidate detected");
        handleABcardType(status, userData);
        break;

    default:
        LOG_TRACE("Kahroba: unsupported card type=%d",
                  (int)status->card.cardType);

        /* Caller owns the reader and must release it. */
        (void)OOP_CALL(picc(), close);
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_CARD_TYPE_ERROR), "");
        break;
    }
}

/** Begins the existing nfcMgr detection; reader retained for EMV APDUs. */
void getNfcData(void) {
    nfcCfg = (NfcCfg){.technologies      = PICC_TECH_A | PICC_TECH_B,
                      .keepOpenOnSuccess = true};
    if (!nfcmgr()->run(&nfcCfg, onNfcComplete, txnMgr())) {
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_ERROR), "");
    }
}
