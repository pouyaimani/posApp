
#include "emv_terminal_hooks.h"
#include "sys/sys.h"
#include "settings/settings.h"

/*
 * TODO(PRODUCTION):
 * Replace these legacy values with actual EMV processing results.
 *
 * TVR must reflect terminal verification.
 * CVM must reflect the actual cardholder verification performed.
 *
 * These constants are retained ONLY for compatibility testing
 * against the legacy Kahroba implementation.
 */
static const uint8_t KAHROBA_LEGACY_TVR[5] = {0x80, 0x00, 0x00, 0x00, 0x00};

static const uint8_t KAHROBA_LEGACY_CVM[3] = {0x24, 0x00, 0x02};

/*
 * TODO(SECURITY - PRODUCTION BLOCKER):
 * Replace this fixed value with four bytes from the device's
 * cryptographically secure random-number generator.
 *
 * A fixed unpredictable number is not suitable for production EMV
 * cryptogram generation.
 */
static const uint8_t KAHROBA_LEGACY_UN[4] = {0xF0, 0x54, 0xBB, 0x66};

/**
 * @brief Get terminal verification and CVM results.
 *
 * Temporary implementation matching the legacy Kahroba flow.
 */
bool txnEmvGetVerifiedChecks(const TxnMgr* txn, uint8_t tvr[5],
                             uint8_t cvm[3]) {
    (void)txn;

    if (tvr == NULL || cvm == NULL)
        return false;

    /* TODO: Calculate TVR and CVM from real EMV processing. */
    memcpy(tvr, KAHROBA_LEGACY_TVR, sizeof(KAHROBA_LEGACY_TVR));
    memcpy(cvm, KAHROBA_LEGACY_CVM, sizeof(KAHROBA_LEGACY_CVM));

    return true;
}

/**
 * @brief Get the EMV unpredictable number (9F37).
 *
 * Temporary legacy-compatible implementation.
 */
bool txnEmvGetUnpredictableNumber(uint8_t output[4]) {
    if (output == NULL)
        return false;

    /* TODO: Generate a fresh secure random number per transaction. */
    memcpy(output, KAHROBA_LEGACY_UN, sizeof(KAHROBA_LEGACY_UN));

    return true;
}

bool emvGetMerchantName(char* name, size_t size) {
    const char* source;
    size_t      len;

    if (name == NULL || size == 0u)
        return false;

    name[0] = '\0';
    source  = settings()->terminal.merchantName;

    /* Source must be a valid null-terminated settings string. */
    if (source == NULL)
        return false;

    len = strlen(source);

    /* Reserve one byte for the null terminator. */
    if (len >= size)
        return false;

    // memcpy(name, source, len + 1u);

    strcpy(name, "something");

    return true;
}