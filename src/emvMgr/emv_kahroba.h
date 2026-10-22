#ifndef EMV_KAHROBA_H
#define EMV_KAHROBA_H

#include "emv_tag_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Kahroba AID from the supplied legacy getNFC() implementation. */
#define EMV_KAHROBA_AID_LEN 7u
extern const uint8_t EMV_KAHROBA_AID[EMV_KAHROBA_AID_LEN];

/** Generate cryptographically strong unpredictable bytes; true on success. */
typedef bool (*EmvRandomFillFn)(void *context, uint8_t *output, size_t length);

/**
 * Terminal parameters for one Kahroba purchase or inquiry.
 *
 * Currency/country are ISO numeric values (e.g. 364 -> BCD 03 64).
 * The deployed switch's actual currency and scheme mappings MUST be verified.
 * TVR and CVM must be real, validated results of terminal processing, never
 * constants copied from the legacy example.
 */
typedef struct {
    uint64_t amount_minor;        /**< Authorized amount in configured minor units. */
    uint64_t other_amount_minor;  /**< Cash-back/other amount, zero if not applicable. */
    uint16_t currency_numeric;    /**< 0..999: transaction currency (5F2A). */
    uint16_t country_numeric;     /**< 0..999: terminal country (9F1A). */
    uint8_t date_bcd[3];          /**< Transaction date YYMMDD (9A). */
    uint8_t time_bcd[3];          /**< Transaction time HHMMSS (9F21). */
    uint8_t transaction_type;     /**< EMV transaction type (9C). */
    uint8_t terminal_type;        /**< Terminal type (9F35). */
    uint8_t tvr[5];               /**< Terminal verification results (95). */
    uint8_t cvm[3];               /**< CVM results (9F34). */
    bool terminal_checks_valid;   /**< Assert 95 was determined, not defaulted. */
    bool cvm_result_valid;        /**< Assert 9F34 was determined, not defaulted. */
    const uint8_t *merchant_name; /**< Optional terminal merchant name (9F4E). */
    size_t merchant_name_len;     /**< Bytes in merchant_name, not NUL-terminated. */
    const uint8_t *entry_data;    /**< Optional 4-byte 9F6E, host-specific. */
} EmvKahrobaTerminal;

/**
 * Build an owned terminal tag database and create a fresh 9F37 using rng.
 * Returns a nonzero error when the source of unpredictable bytes fails.
 */
EmvError emvKahrobaPrepareTerminal(EmvTagDb *tags,
                                   const EmvKahrobaTerminal *terminal,
                                   EmvRandomFillFn rng, void *rng_context);

/**
 * Build the proprietary GPO/TTPI payload observed in the supplied legacy code.
 * Fixed capability bytes and data order MUST be certified against the Kahroba
 * terminal specification; this is a test/reference implementation only.
 */
EmvError emvKahrobaBuildGpo(const EmvTagDb *tags,
                            uint8_t *output, size_t capacity,
                            size_t *written);

#ifdef __cplusplus
}
#endif
#endif
