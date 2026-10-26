#ifndef EMV_TERMINAL_HOOKS_H
#define EMV_TERMINAL_HOOKS_H

#include <stdbool.h>
#include <stdint.h>
#include "txnMgr.h"
/*
 * Application-provided policy/time functions.
 * Their definitions belong in the application, not the portable EMV core.
 */
/*
 * Convert the transaction amount to the units used by EMV tag 9F02 and
 * return the scheme/acquirer-approved numeric currency code for 5F2A.
 * Return false until the amount unit and currency mapping are confirmed.
 */
bool txnEmvGetPaymentParameters(const TxnMgr* txn, uint64_t* amount_minor,
                                uint16_t* currency_numeric);

/*
 * Supply real, scheme-compliant TVR (95) and CVM Results (9F34).
 * Do NOT return true for arbitrary defaults copied from legacy code.
 * NOTE: In many EMV flows these cannot be known before card processing:
 * in that case the Kahroba core must be refactored to calculate/update
 * these tags later, before constructing CDOL1 and Field 55.
 */
bool txnEmvGetVerifiedChecks(const TxnMgr* txn, uint8_t tvr[5], uint8_t cvm[3]);

bool emvGetMerchantName(char* name, size_t size);

/**
 * @brief Get the EMV unpredictable number (9F37).
 *
 * Temporary legacy-compatible implementation.
 */
bool txnEmvGetUnpredictableNumber(uint8_t output[4]);

#endif
