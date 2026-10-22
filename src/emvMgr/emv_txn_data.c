#include "emv_txn_data.h"
#include <string.h>

/** Prevent optimization from removing erasure of cardholder data. */
void emvTxnDataClear(EmvTxnData *data) {
    volatile uint8_t *p;
    size_t n;
    if (data == NULL) { return; }
    p = (volatile uint8_t *)data;
    n = sizeof(*data);
    while (n-- != 0u) { *p++ = 0u; }
}

/** Copy a tag if present, refusing unexpectedly long card data. */
static EmvError copy_tag(const EmvTagDb *db, EmvTag tag,
                         uint8_t *dst, size_t capacity, size_t *written,
                         bool required, size_t exact_len) {
    const uint8_t *value = NULL;
    size_t len = 0u;
    *written = 0u;
    if (!emvTagDbGet(db, tag, &value, &len)) {
        return required ? EMV_ERR_MISSING_TAG : EMV_OK;
    }
    if (len > capacity || (exact_len != 0u && len != exact_len)) {
        return EMV_ERR_PROTOCOL;
    }
    if (len != 0u) { memcpy(dst, value, len); }
    *written = len;
    return EMV_OK;
}

EmvError emvTxnDataImport(EmvTxnData *destination,
                          const EmvResult *result,
                          const uint8_t *field55, size_t field55_len) {
    EmvError err;
    size_t n = 0u;
    if (destination == NULL) { return EMV_ERR_ARGUMENT; }
    emvTxnDataClear(destination);
    if (result == NULL || result->outcome != EMV_OUTCOME_ONLINE_REQUIRED ||
        result->error != EMV_OK || result->transaction_id == 0u ||
        result->session_id == 0u ||
        (field55_len != 0u && field55 == NULL) ||
        field55_len == 0u || field55_len > sizeof(destination->field55)) {
        return EMV_ERR_ARGUMENT;
    }
    /* Result tags are copied rather than referenced; result may be reset. */
    err = copy_tag(&result->tags, 0x9F26u, destination->cryptogram,
                   sizeof(destination->cryptogram), &n, true, 8u);
    if (err != EMV_OK) { goto fail; }
    err = copy_tag(&result->tags, 0x9F36u, destination->atc,
                   sizeof(destination->atc), &n, true, 2u);
    if (err != EMV_OK) { goto fail; }
    err = copy_tag(&result->tags, 0x9F27u, &destination->cid,
                   1u, &n, true, 1u);
    if (err != EMV_OK || (destination->cid & 0xC0u) != 0x80u) {
        err = EMV_ERR_PROTOCOL;
        goto fail;
    }
    err = copy_tag(&result->tags, 0x84u, destination->aid,
                   sizeof(destination->aid), &destination->aid_len, true, 0u);
    if (err != EMV_OK || destination->aid_len == 0u) {
        err = EMV_ERR_PROTOCOL;
        goto fail;
    }
    err = copy_tag(&result->tags, 0x5Au, destination->pan_bcd,
                   sizeof(destination->pan_bcd), &destination->pan_bcd_len, false, 0u);
    if (err != EMV_OK) { goto fail; }
    err = copy_tag(&result->tags, 0x57u, destination->track2_bcd,
                   sizeof(destination->track2_bcd), &destination->track2_bcd_len, false, 0u);
    if (err != EMV_OK) { goto fail; }
    memcpy(destination->field55, field55, field55_len);
    destination->field55_len = field55_len;
    return EMV_OK;
fail:
    emvTxnDataClear(destination);
    return err;
}
