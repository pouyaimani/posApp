#include "txnMgr.h"
#include "logger.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "inputMgr/inputMgr.h"
#include "ui/infoPage.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "infoMgr/infoMgr.h"
#include "display/display.h"
#include "menuMgr/menuMgr.h"

/*
 * Persistent input configuration and output storage.
 *
 * The configuration remains valid while InputMgr is running, even if
 * InputMgr retains its pointer rather than copying the structure.
 *
 * Reserve an additional byte for the amount string terminator.
 */
static InputCfg inputCfg;

void onSimCharge();

/******************************************************************
 *                      Input result handling
 ******************************************************************/

/*
 * Recognize only input steps currently connected to these callbacks.
 *
 * Extend this function when the remaining input handlers are
 * implemented and connected to onInputComplete().
 *
 * This is a step guard, not protection against callbacks belonging
 * to an older transaction. InputMgr must stop delivering old events
 * before a new transaction starts.
 */
bool isInputStep(const TxnMgr* mgr) {
    if (!mgr || !mgr->active || !mgr->stepStarted) {
        return false;
    }
    return mgr->step == TXN_STEP_AMOUNT || mgr->step == TXN_STEP_PIN ||
           mgr->step == TXN_STEP_BILL_ID || mgr->step == TXN_STEP_PAYMENT_ID ||
           mgr->step == TXN_STEP_OPERATOR || mgr->step == TXN_STEP_PHONE ||
           mgr->step == TXN_STEP_CHARGE_AMOUNT;
}

void amountErrInfoDone(InfoResult res, void* arg) {
    LOG_TRACE("TxnMgr: amount error screen closed; restarting amount input");
    txnMgr()->retry();
}

void billIdErrInfoDone(InfoResult res, void* arg) {
    LOG_TRACE("TxnMgr: amount error screen closed; restarting amount input");
    txnMgr()->retry();
}

void payIdErrInfoDone(InfoResult res, void* arg) {
    LOG_TRACE("TxnMgr: amount error screen closed; restarting amount input");
    txnMgr()->previous();
}

/*
 * Validate and store the amount before advancing to PIN.
 *
 * The minimum-length rule preserves the existing behavior.
 * Add transaction-specific numeric limits here when available.
 */
bool validateAmount(TxnMgr* mgr) {
    uint64_t amount = 0;
    size_t   len    = 0;

    if (!mgr) {
        LOG_TRACE("TxnMgr: amount validation rejected: null manager");
        return false;
    }

    if (strlen(inmgr()->input) < LEN_MIN_AMOUNT) {
        LOG_TRACE("TxnMgr: amount validation failed: below minimum length");
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = amountErrInfoDone},
                       phraseGetDef(PHRASE_AMOUNT_FLOOR_ER), "");
        return false;
    }

    if (strlen(inmgr()->input) > LEN_MAX_AMOUNT) {
        LOG_TRACE("TxnMgr: amount validation failed: above maximum length");
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = amountErrInfoDone},
                       phraseGetDef(PHRASE_AMOUNT_EXCEED), "");
        return false;
    }

    if (!str2u64(inmgr()->input, &amount)) {
        LOG_TRACE("TxnMgr: amount validation failed: conversion error");
        LOG_FATAL("Converting amount form string to u64 failed.");
        return false;
    }

    /* TODO: Apply numeric minimum/maximum rules for your payment protocol. */
    mgr->data.core.amount = amount;
    LOG_TRACE("TxnMgr: amount validated and stored");
    return true;
}

bool validateBillId(TxnMgr* mgr) {
    LOG_TRACE("Bill id = %s", inmgr()->input);
    if (!isBillIdValid(inmgr()->input)) {
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = billIdErrInfoDone},
                       phraseGetDef(PHRASE_BILL_INVALID), "");
        return false;
    }
    memcpy(mgr->data.extention.bill.billId, inmgr()->input,
           sizeof(mgr->data.extention.bill.billId));
    return true;
}

bool validatePaymentId(TxnMgr* mgr) {
    LOG_TRACE("PaymentId id = %s", inmgr()->input);
    if (!isBillValid(mgr->data.extention.bill.billId, inmgr()->input)) {
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = payIdErrInfoDone},
                       phraseGetDef(PHRASE_BILL_INVALID), "");
        return false;
    }
    memcpy(mgr->data.extention.bill.paymentId, inmgr()->input,
           sizeof(mgr->data.extention.bill.paymentId));
    DEFINE_STRING(amountStr, LEN_MAX_AMOUNT);
    billExtractAmount(mgr->data.extention.bill.paymentId, amountStr,
                      sizeof(amountStr));
    static uint64_t amount;
    if (!str2u64(amountStr, &amount)) {
        LOG_FATAL("Converting amount form string to u64 failed.");
        return false;
    }
    mgr->data.core.amount = amount;
    return true;
}

void setPhoneNum(TxnMgr* mgr) {
    memcpy(mgr->data.extention.charge.phoneNumber, inmgr()->input,
           sizeof(mgr->data.extention.charge.phoneNumber));
}

bool onPinEntryDone(TxnMgr* mgr) {}

/*
 * Handle completion from the currently active input provider.
 *
 * InputMgr must commit its output buffer before invoking this
 * callback. Its provider cleanup must also permit starting another
 * input operation from next().
 */
void onInputComplete(InputResult result, void* userData) {
    TxnMgr* mgr = userData;

    if (!isInputStep(mgr)) {
        LOG_TRACE(
            "TxnMgr: ignoring input callback outside an active input step");
        return;
    }

    switch (result) {
    case INPUT_RES_FINISHED:
        LOG_TRACE("TxnMgr: input finished; step=%d", (int)mgr->step);
        switch (mgr->step) {
        case TXN_STEP_AMOUNT:
            if (!validateAmount(mgr)) {
                return;
            }
            break;
        case TXN_STEP_BILL_ID:
            if (!validateBillId(mgr)) {
                return;
            }
            break;
        case TXN_STEP_PAYMENT_ID:
            if (!validatePaymentId(mgr)) {
                return;
            }
            break;
        case TXN_STEP_OPERATOR:
            break;
        case TXN_STEP_CHARGE_AMOUNT:
            break;
        case TXN_STEP_PHONE:
            setPhoneNum(mgr);
            break;

        case TXN_STEP_PIN:
            /*
             * PIN processing belongs to the PED provider.
             * Do not copy plaintext PIN input into transaction data.
             *
             * TODO:
             * Confirm that INPUT_RES_FINISHED means the required
             * PED result is ready for transaction composition.
             */
            break;

        default:
            return;
        }

        mgr->next();
        return;

    case INPUT_RES_CANCELED:
        LOG_TRACE("TxnMgr: input canceled; step=%d", (int)mgr->step);
        mgr->fail(TXN_MGR_ERR_CANCELED);
        return;

    default:
        /* Processing notifications do not complete the step. */
        return;
    }
}

/*
 * Report timeout through the same failure path as other errors.
 * The application's onFailure callback owns result navigation.
 */
void onInputTimeout(InputResult result, void* userData) {
    TxnMgr* mgr = userData;

    (void)result;

    if (!isInputStep(mgr)) {
        LOG_TRACE("TxnMgr: ignoring timeout outside an active input step");
        return;
    }

    LOG_TRACE("TxnMgr: input timeout; step=%d", (int)mgr->step);
    mgr->fail(TXN_MGR_ERR_TIMEOUT);
}

/******************************************************************
 *                         Input steps
 ******************************************************************/

void enterAmount(void) {
    LOG_TRACE("TxnMgr: starting amount input");
    inputCfg = (InputCfg){.mode    = INMD_ENTER_AMOUNT,
                          .title   = phraseGetDef(PHRASE_AMOUNT),
                          .info    = phraseGetDef(PHRASE_TO_RIAL_1),
                          .maxLen  = LEN_MAX_AMOUNT,
                          .timeout = BACK_IDLE_TMO,
                          .tocb    = onInputTimeout,
                          .cbData  = txnMgr()};

    inmgr()->run(&inputCfg, onInputComplete, txnMgr());
}

void enterBillId(void) {
    inputCfg = (InputCfg){.mode    = INMD_ENTER_NUMBERS,
                          .title   = phraseGetDef(PHRASE_BILL_ID),
                          .info    = "",
                          .maxLen  = LEN_MAX_BILL_ID,
                          .timeout = BACK_IDLE_TMO,
                          .tocb    = onInputTimeout,
                          .cbData  = txnMgr()};

    inmgr()->run(&inputCfg, onInputComplete, txnMgr());
}

void enterPaymentId(void) {
    inputCfg = (InputCfg){.mode    = INMD_ENTER_NUMBERS,
                          .title   = phraseGetDef(PHRASE_PAYMENT_ID),
                          .info    = "",
                          .maxLen  = LEN_MAX_PAYMENT_ID,
                          .timeout = BACK_IDLE_TMO,
                          .tocb    = onInputTimeout,
                          .cbData  = txnMgr()};

    inmgr()->run(&inputCfg, onInputComplete, txnMgr());
}

void setOperator(void* arg) {
    int idx = *(((int*)arg));
    LOG_TRACE("setOperator(): idx = %d", idx);
    SimCardOp_t selectedOp;
    switch (idx) {
    case 0:
        selectedOp = OPERATOR_MCI;
        break;
    case 1:
        selectedOp = OPERATOR_MTN;
        break;
    case 2:
        selectedOp = OPERATOR_RIGHTEL;
        break;
    default:
        break;
    }
    txnMgr()->data.extention.charge.op = selectedOp;
    onInputComplete(INPUT_RES_FINISHED, txnMgr());
}

static Menu       menu;
static MenuMgrCfg menucfg;


static txnCancel(void* arg) { txnMgr()->fail(TXN_MGR_ERR_CANCELED); }

void selectOperator(void) {
    menucfg        = menuCfgBack2idleOBC;
    menucfg.onExit = txnCancel;
    ui_menu_create(&menu, disp()->screen);
    ui_menu_addItem(&menu, phraseGetDef(PHRASE_SIM_OP_MCI), LV_TEXT_ALIGN_RIGHT,
                    NULL, setOperator, NULL);
    ui_menu_addItem(&menu, phraseGetDef(PHRASE_SIM_OP_MTN), LV_TEXT_ALIGN_RIGHT,
                    NULL, setOperator, NULL);
    ui_menu_addItem(&menu, phraseGetDef(PHRASE_SIM_OP_RIGHTEL),
                    LV_TEXT_ALIGN_RIGHT, NULL, setOperator, NULL);
    menuMgr()->run(&menu, NULL, &menucfg);
}

static desiredAmnt[LEN_MAX_AMOUNT + 1];

static const uint64_t amnt[] = {
    200000, // 0
    500000, // 1
    1000000 // 2
};

static void setAmnt(void* arg) {
    txnMgr()->data.core.amount = *((uint64_t*)arg);
    onInputComplete(INPUT_RES_FINISHED, txnMgr());
}

void setChargeAmnt() {
    // 5-10-20-50
    ui_menu_addItem(&menu, "200,000", LV_TEXT_ALIGN_LEFT, NULL, setAmnt,
                    &amnt[0]);
    ui_menu_addItem(&menu, "500,000", LV_TEXT_ALIGN_LEFT, NULL, setAmnt,
                    &amnt[1]);
    ui_menu_addItem(&menu, "1,000,000", LV_TEXT_ALIGN_LEFT, NULL, setAmnt,
                    &amnt[2]);
    ui_menu_addItem(&menu, phraseGetDef(PHRASE_ENTER_CHARGE_AMNT),
                    LV_TEXT_ALIGN_LEFT, NULL, enterAmount, NULL);
}

void enterChargeAmount(void) {
    ui_menu_create(&menu, disp()->screen);
    setChargeAmnt();
    menucfg        = menuCfgBack2idleOBC;
    menucfg.onExit = txnCancel;
    menuMgr()->run(&menu, NULL, &menucfg);
}

void enterPhone(void) {
    memset(&inFinishCtx, 0, sizeof(InputFinishCtx));
    inmgr()->run(&(InputCfg){.type    = INPUT_TYPE_KEYPAD,
                             .mode    = INMD_ENTER_NUMBERS,
                             .title   = phraseGetDef(PHRASE_PHONE_NUMBER),
                             .info    = "",
                             .maxLen  = LEN_MAX_PHONE_NUMBER,
                             .timeout = BACK_IDLE_TMO,
                             .tocb    = back2idle},
                 onInputComplete, txnMgr());
    inmgr()->set(INPUT_TYPE_KEYPAD, "09");
}

void enterPin(void) {
    LOG_TRACE("TxnMgr: starting PED PIN input");
    inputCfg = (InputCfg){.type    = INPUT_TYPE_PED,
                          .mode    = INMD_ENTER_PIN,
                          .title   = phraseGetDef(PHRASE_CARD_PIN),
                          .info    = phraseGetDef(PHRASE_BY_CUSTOMER),
                          .maxLen  = LEN_MAX_CARD_PIN,
                          .timeout = BACK_IDLE_TMO,
                          .tocb    = onInputTimeout,
                          .cbData  = txnMgr()};

    /*
     * TODO:
     * Supply the balance fee message here if required.
     * Keep any dynamically formatted message valid for the complete
     * lifetime of the input screen.
     */

    inmgr()->run(&inputCfg, onInputComplete, txnMgr());
}
