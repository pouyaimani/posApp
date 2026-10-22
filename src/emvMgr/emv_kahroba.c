#include "emv_kahroba.h"
#include <string.h>

const uint8_t EMV_KAHROBA_AID[EMV_KAHROBA_AID_LEN] = {
    0xA0u, 0x00u, 0x00u, 0x08u, 0x03u, 0x00u, 0x01u
};

/** Encode a decimal integer as fixed-width packed BCD, rejecting overflow. */
static bool pack_decimal(uint64_t value, uint8_t *dst, size_t bytes) {
    size_t i;
    for (i = bytes; i != 0u; ) {
        uint8_t lo, hi;
        --i;
        lo = (uint8_t)(value % 10u); value /= 10u;
        hi = (uint8_t)(value % 10u); value /= 10u;
        dst[i] = (uint8_t)((hi << 4u) | lo);
    }
    return value == 0u;
}

/** Reject invalid packed date/time values before building terminal tags. */
static bool bcd_ok(const uint8_t *bcd, size_t count) {
    size_t i;
    for (i = 0u; i < count; ++i) {
        if ((bcd[i] & 0x0Fu) > 9u || (bcd[i] >> 4u) > 9u) { return false; }
    }
    return true;
}

static EmvError put(EmvTagDb *db, EmvTag tag, const void *data, size_t len) {
    return emvTagDbSet(db, tag, (const uint8_t *)data, len);
}

EmvError emvKahrobaPrepareTerminal(EmvTagDb *db,
                                   const EmvKahrobaTerminal *t,
                                   EmvRandomFillFn rng, void *rng_context) {
    uint8_t amount[6], other[6], curr[2], country[2], unpredictable[4];
    EmvError err = EMV_OK;
    if (db == NULL) { return EMV_ERR_ARGUMENT; }
    /* Ensure even a rejected start cannot leave stale data in caller storage. */
    emvTagDbClear(db);
    if (t == NULL || rng == NULL ||
        !t->terminal_checks_valid || !t->cvm_result_valid ||
        t->currency_numeric == 0u || t->country_numeric == 0u ||
        t->currency_numeric > 999u || t->country_numeric > 999u ||
        t->merchant_name_len > 40u ||
        (t->merchant_name_len != 0u && t->merchant_name == NULL) ||
        !bcd_ok(t->date_bcd, sizeof(t->date_bcd)) ||
        !bcd_ok(t->time_bcd, sizeof(t->time_bcd)) ||
        t->date_bcd[1] == 0u || t->date_bcd[1] > 0x12u ||
        t->date_bcd[2] == 0u || t->date_bcd[2] > 0x31u ||
        t->time_bcd[0] > 0x23u || t->time_bcd[1] > 0x59u ||
        t->time_bcd[2] > 0x59u) {
        return EMV_ERR_ARGUMENT;
    }
    if (!pack_decimal(t->amount_minor, amount, sizeof(amount)) ||
        !pack_decimal(t->other_amount_minor, other, sizeof(other)) ||
        !pack_decimal(t->currency_numeric, curr, sizeof(curr)) ||
        !pack_decimal(t->country_numeric, country, sizeof(country))) {
        return EMV_ERR_ARGUMENT;
    }
    if (!rng(rng_context, unpredictable, sizeof(unpredictable))) {
        return EMV_ERR_RANDOM;
    }
#define PUT(TAG, VALUE, LEN) do { \
        err = put(db, TAG, VALUE, LEN); \
        if (err != EMV_OK) { goto cleanup; } \
    } while (0)
    PUT(0x9F02u, amount, sizeof(amount));
    PUT(0x9F03u, other, sizeof(other));
    PUT(0x5F2Au, curr, sizeof(curr));
    PUT(0x9F1Au, country, sizeof(country));
    PUT(0x9Au, t->date_bcd, sizeof(t->date_bcd));
    PUT(0x9F21u, t->time_bcd, sizeof(t->time_bcd));
    PUT(0x9Cu, &t->transaction_type, 1u);
    PUT(0x9F35u, &t->terminal_type, 1u);
    PUT(0x95u, t->tvr, sizeof(t->tvr));
    PUT(0x9F34u, t->cvm, sizeof(t->cvm));
    PUT(0x9F37u, unpredictable, sizeof(unpredictable));
    if (t->merchant_name_len != 0u) {
        PUT(0x9F4Eu, t->merchant_name, t->merchant_name_len);
    }
    if (t->entry_data != NULL) { PUT(0x9F6Eu, t->entry_data, 4u); }
#undef PUT
cleanup:
    memset(unpredictable, 0, sizeof(unpredictable));
    if (err != EMV_OK) { emvTagDbClear(db); }
    return err;
}

/** Fetch a terminal-owned EMV tag with exact required length. */
static EmvError required(const EmvTagDb *db, EmvTag tag,
                         size_t length, uint8_t *dst) {
    const uint8_t *value = NULL;
    size_t len = 0u;
    if (!emvTagDbGet(db, tag, &value, &len) || len != length) {
        return EMV_ERR_MISSING_TAG;
    }
    memcpy(dst, value, len);
    return EMV_OK;
}

EmvError emvKahrobaBuildGpo(const EmvTagDb *tags,
                            uint8_t *output, size_t capacity,
                            size_t *written) {
    static const uint8_t flags[5] = {0x34u, 0x00u, 0x40u, 0x02u, 0x88u};
    EmvError err;
    if (tags == NULL || output == NULL || written == NULL) {
        return EMV_ERR_ARGUMENT;
    }
    *written = 0u;
    if (capacity < 19u) { return EMV_ERR_CAPACITY; }
    output[0] = 0x83u; output[1] = 17u;
    memcpy(output + 2u, flags, sizeof(flags));
    if ((err = required(tags, 0x9F02u, 6u, output + 7u)) != EMV_OK ||
        (err = required(tags, 0x5F2Au, 2u, output + 13u)) != EMV_OK ||
        (err = required(tags, 0x9F1Au, 2u, output + 15u)) != EMV_OK ||
        (err = required(tags, 0x9F35u, 1u, output + 17u)) != EMV_OK ||
        (err = required(tags, 0x9Cu, 1u, output + 18u)) != EMV_OK) {
        return err;
    }
    *written = 19u;
    return EMV_OK;
}
