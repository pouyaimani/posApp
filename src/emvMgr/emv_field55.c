#include "emv_field55.h"
#include <string.h>

static size_t tag_width(EmvTag tag) {
    return tag <= 0xFFu ? 1u : (tag <= 0xFFFFu ? 2u : (tag <= 0xFFFFFFu ? 3u : 0u));
}

EmvError emvField55Build(const EmvTagDb *tags,
                         const EmvField55Spec *spec, size_t spec_count,
                         uint8_t *output, size_t output_capacity,
                         size_t *written) {
    size_t i, used = 0u;
    if (tags == NULL || output == NULL || written == NULL ||
        (spec_count != 0u && spec == NULL)) { return EMV_ERR_ARGUMENT; }
    *written = 0u;
    for (i = 0u; i < spec_count; ++i) {
        const uint8_t *value = NULL;
        size_t len = 0u, tw = tag_width(spec[i].tag), lw, needed, k;
        if (tw == 0u || spec[i].tag == 0u) { return EMV_ERR_ARGUMENT; }
        if (!emvTagDbGet(tags, spec[i].tag, &value, &len)) {
            if (spec[i].required) { return EMV_ERR_MISSING_TAG; }
            continue;
        }
        if (len > 65535u) { return EMV_ERR_CAPACITY; }
        lw = len <= 127u ? 1u : (len <= 255u ? 2u : 3u);
        needed = tw + lw + len;
        if (needed > output_capacity - used) { return EMV_ERR_CAPACITY; }
        for (k = 0u; k < tw; ++k) {
            output[used++] = (uint8_t)(spec[i].tag >> (8u * (tw - k - 1u)));
        }
        if (lw == 1u) { output[used++] = (uint8_t)len; }
        else if (lw == 2u) {
            output[used++] = 0x81u;
            output[used++] = (uint8_t)len;
        } else {
            output[used++] = 0x82u;
            output[used++] = (uint8_t)(len >> 8u);
            output[used++] = (uint8_t)len;
        }
        if (len != 0u) { memcpy(output + used, value, len); }
        used += len;
    }
    *written = used;
    return EMV_OK;
}
