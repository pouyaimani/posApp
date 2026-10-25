// #include "emvMgr/txn_emv_sync_port.h"
// #include "emvMgr/txn_emv_terminal_hooks.h"
// #include "settings/settings.h"
// #include "logger.h"

// #include <string.h>
// #include <stddef.h>
// #include <stdint.h>

// /**
//  * Get the length of a fixed-size string safely, without using non-C11
//  strnlen.
//  * Returns capacity if no NUL terminator occurs within the buffer.
//  */
// static size_t emvBoundedStrlen(const char* text, size_t capacity) {
//     size_t i;
//     if (text == NULL) {
//         return capacity;
//     }
//     for (i = 0u; i < capacity; ++i) {
//         if (text[i] == '\0') {
//             break;
//         }
//     }
//     return i;
// }

// /** Verify that each packed-BCD nibble represents a decimal digit. */
// static bool emvValidBcd(const uint8_t* value, size_t length) {
//     size_t i;
//     if (value == NULL) {
//         return false;
//     }
//     for (i = 0u; i < length; ++i) {
//         if ((value[i] & 0x0Fu) > 9u || (value[i] >> 4u) > 9u) {
//             return false;
//         }
//     }
//     return true;
// }

// /** Validate basic RTC ranges; detailed calendar validation may be in RTC
// HAL.
//  */
// static bool emvValidDateTime(const uint8_t date[3], const uint8_t time[3]) {
//     if (!emvValidBcd(date, 3u) || !emvValidBcd(time, 3u)) {
//         return false;
//     }
//     if (date[1] < 0x01u || date[1] > 0x12u || date[2] < 0x01u ||
//         date[2] > 0x31u || time[0] > 0x23u || time[1] > 0x59u ||
//         time[2] > 0x59u) {
//         return false;
//     }
//     return true;
// }

// /**
//  * Prepare device-independent terminal input for Kahroba purchase processing.
//  *
//  * The merchant-name pointer borrows stable memory from settings(); callers
//  * must keep settings immutable until emvKahrobaPrepareTerminal() copies it.
//  * On any failure *terminal remains zero-initialized.
//  *
//  * Preconditions: txn->data.core.amount contains a validated transaction
//  amount;
//  * txnEmvGetPaymentParameters converts it to the confirmed 9F02 amount unit.
//  */
// bool txnEmvBuildTerminal(const TxnMgr* txn, EmvKahrobaTerminal* terminal) {
//     EmvKahrobaTerminal tmp;
//     DevSettings*       dev;
//     size_t             name_len;

//     if (terminal == NULL) {
//         LOG_TRACE("EMV: terminal build failed: null output");
//         return false;
//     }
//     memset(terminal, 0, sizeof(*terminal));

//     if (txn == NULL || !txn->active) {
//         LOG_TRACE("EMV: terminal build failed: inactive/null transaction");
//         return false;
//     }

//     /* This implementation follows the legacy Kahroba PURCHASE flow only. */
//     if (txn->data.core.txnType != TXN_KAHROBA_PURCHASE) {
//         LOG_TRACE("EMV: terminal build failed: unsupported txn type=%d",
//                   (int)txn->data.core.txnType);
//         return false;
//     }

//     dev = settings();
//     if (dev == NULL) {
//         LOG_TRACE("EMV: terminal build failed: settings unavailable");
//         return false;
//     }

//     memset(&tmp, 0, sizeof(tmp));

//     /* 9F02 and 5F2A: depends on approved amount-unit/currency mapping. */
//     if (!txnEmvGetPaymentParameters(txn, &tmp.amount_minor,
//                                     &tmp.currency_numeric) ||
//         tmp.currency_numeric == 0u || tmp.currency_numeric > 999u ||
//         tmp.amount_minor == 0u || tmp.amount_minor > UINT64_C(999999999999))
//         { LOG_TRACE("EMV: terminal build failed: amount/currency
//         configuration"); return false;
//     }

//     /* Purchase without cashback (9F03). */
//     tmp.other_amount_minor = 0u;

//     /* Legacy Kahroba terminal country 9F1A=0364 (Iran). */
//     tmp.country_numeric = 364u;

//     /* 9C=purchase; 9F35=attended online-only merchant terminal. */
//     tmp.transaction_type = 0x00u;
//     tmp.terminal_type    = 0x21u;

//     /* Use RTC; do not guess the encoding of txn->data.dateTime. */
//     if (!txnEmvGetDateTimeBcd(tmp.date_bcd, tmp.time_bcd) ||
//         !emvValidDateTime(tmp.date_bcd, tmp.time_bcd)) {
//         LOG_TRACE("EMV: terminal build failed: invalid RTC date/time");
//         return false;
//     }

//     /* Borrow the NUL-terminated merchant name from in-memory settings. */
//     name_len = emvBoundedStrlen(dev->terminal.merchantName,
//                                 sizeof(dev->terminal.merchantName));
//     if (name_len == sizeof(dev->terminal.merchantName)) {
//         LOG_TRACE("EMV: terminal build failed: merchant name not
//         terminated"); return false;
//     }
//     if (name_len != 0u) {
//         tmp.merchant_name     = (const uint8_t*)dev->terminal.merchantName;
//         tmp.merchant_name_len = name_len;
//     }

//     /*
//      * Legacy 9F6E was a constant, but its meaning is scheme/host specific.
//      * Omit until the acquirer confirms the actual required value.
//      */
//     tmp.entry_data = NULL;

//     /*
//      * These flags MUST NOT be forced true with hardcoded legacy values.
//      * If TVR/CVM depend on card data, calculate them later in emvKahroba;
//      * until then reject the transaction rather than constructing bad CDOL1.
//      */
//     if (!txnEmvGetVerifiedChecks(txn, tmp.tvr, tmp.cvm)) {
//         LOG_TRACE("EMV: terminal build failed: TVR/CVM not established");
//         return false;
//     }
//     tmp.terminal_checks_valid = true;
//     tmp.cvm_result_valid      = true;

//     *terminal = tmp;
//     LOG_TRACE("EMV: Kahroba terminal ready; txnType=%d, merchantLen=%u",
//               (int)txn->data.core.txnType, (unsigned)name_len);
//     /* Never log the full EMV payload, PAN, track 2 or cardholder data. */
//     return true;
// }

// /** Use the device's approved cryptographically secure random-number source.
// */ bool txnEmvRandom(void* context, uint8_t* bytes, size_t length) {}

// /** Copy validated result and binary Field 55 into transaction-owned memory.
// */ bool txnEmvImportResult(TxnMgr* txn, const EmvResult* result,
//                         const uint8_t* field55, size_t field55Length) {}

// /** Map EMV failure to the exi   sting TxnMgr's error/status model. */
// void txnEmvFailTransaction(TxnMgr* txn, EmvOutcome outcome, EmvError error)
// {}

// /*
//  * Application-provided policy/time functions.
//  * Their definitions belong in the application, not the portable EMV core.
//  */
// /*
//  * Convert the transaction amount to the units used by EMV tag 9F02 and
//  * return the scheme/acquirer-approved numeric currency code for 5F2A.
//  * Return false until the amount unit and currency mapping are confirmed.
//  */
// bool txnEmvGetPaymentParameters(const TxnMgr* txn, uint64_t* amount_minor,
//                                 uint16_t* currency_numeric) {}

// /* Supply CURRENT RTC date YYMMDD and time HHMMSS, packed BCD. */
// bool txnEmvGetDateTimeBcd(uint8_t date_bcd[3], uint8_t time_bcd[3]) {}

// /*
//  * Supply real, scheme-compliant TVR (95) and CVM Results (9F34).
//  * Do NOT return true for arbitrary defaults copied from legacy code.
//  * NOTE: In many EMV flows these cannot be known before card processing:
//  * in that case the Kahroba core must be refactored to calculate/update
//  * these tags later, before constructing CDOL1 and Field 55.
//  */
// bool txnEmvGetVerifiedChecks(const TxnMgr* txn, uint8_t tvr[5],
//                              uint8_t cvm[3]) {}