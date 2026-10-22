#include "emv_dol.h"
#include <string.h>

/* Supplying dummy values for these inputs would corrupt transaction semantics. */
/** Terminal-sourced DOL values that must not be silently zero-filled. */
static bool critical_terminal_tag(EmvTag tag) {
    switch (tag) {
        case 0x9F02u: /* Authorized amount */
        case 0x9F37u: /* Unpredictable number */
        case 0x9F1Au: /* Terminal country */
        case 0x5F2Au: /* Transaction currency */
        case 0x9Au:   /* Transaction date */
        case 0x9Cu:   /* Transaction type */
        case 0x95u:   /* TVR, must come from actual terminal processing */
        case 0x9F34u: /* CVM result, must come from actual CVM processing */
        case 0x9F35u: /* Terminal type */
        case 0x9F21u: /* Time */
            return true;
        default: return false;
    }
}

EmvError emvDolBuild(const uint8_t *dol, size_t dol_len,
                     const EmvTagDb *tags, uint8_t *out, size_t capacity,
                     size_t *written) {
    size_t pos = 0u, used = 0u;
    if (tags == NULL || out == NULL || written == NULL ||
        (dol_len != 0u && dol == NULL)) { return EMV_ERR_ARGUMENT; }
    *written = 0u;
    while (pos < dol_len) {
        EmvTag tag = dol[pos++];
        uint8_t b = (uint8_t)tag;
        size_t field_len, src_len = 0u, copy_len;
        const uint8_t *src = NULL;
        if ((b & 0x1Fu) == 0x1Fu) {
            unsigned count = 0u;
            do {
                if (pos >= dol_len || count >= 2u) { return EMV_ERR_TLV; }
                b = dol[pos++];
                tag = (tag << 8u) | b;
                ++count;
            } while ((b & 0x80u) != 0u);
        }
        if (pos >= dol_len) { return EMV_ERR_TLV; }
        field_len = dol[pos++];
        if (field_len > capacity - used) { return EMV_ERR_CAPACITY; }
        if (!emvTagDbGet(tags, tag, &src, &src_len) && critical_terminal_tag(tag)) {
            return EMV_ERR_MISSING_TAG;
        }
        if (src != NULL && critical_terminal_tag(tag) && src_len != field_len) {
            return EMV_ERR_PROTOCOL;
        }
        memset(out + used, 0, field_len);
        if (tag == 0x9F4Eu) { memset(out + used, 0x20, field_len); }
        if (src != NULL) {
            copy_len = src_len < field_len ? src_len : field_len;
            if (copy_len != 0u) { memcpy(out + used, src, copy_len); }
        }
        used += field_len;
    }
    *written = used;
    return EMV_OK;
}
