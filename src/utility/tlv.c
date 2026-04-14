#include "tlv.h"
#include "dev/dev.h"

static TLV *__tlv;

static uint16_t encode(uint8_t *out,
                    const uint8_t *tag,
                    uint8_t tagLen,
                    const void *value,
                    uint16_t valueLen)
{
    uint16_t offset = 0;

    out[offset++] = 0xFF; // marker

    out[offset++] = tagLen;
    memcpy(out + offset, tag, tagLen);
    offset += tagLen;

    if (valueLen == 0)
        return 0;

    if (valueLen > 255) {
        out[offset++] = 0x82;
        out[offset++] = (uint8_t)(valueLen >> 8);
    } else if (valueLen > 127) {
        out[offset++] = 0x81;
    }

    out[offset++] = (uint8_t)valueLen;

    memcpy(out + offset, value, valueLen);
    offset += valueLen;

    return offset;
}

static int decode(const uint8_t *data,
              uint16_t len,
              TlvCallback cb,
              void *user)
{
    uint16_t offset = 0;

    while (offset < len)
    {
        if (data[offset] == 0xFF) {
            offset++;
            continue;
        }

        // --- TAG ---
        uint8_t tagLen = data[offset++];
        const uint8_t *tag = &data[offset];
        offset += tagLen;

        // --- LENGTH ---
        uint16_t valueLen = 0;

        if (data[offset] <= 127) {
            valueLen = data[offset++];
        } else {
            uint8_t lenBytes = data[offset++] & 0x7F;
            for (int i = 0; i < lenBytes; i++) {
                valueLen = (valueLen << 8) | data[offset++];
            }
        }

        // --- VALUE ---
        const uint8_t *value = &data[offset];
        offset += valueLen;

        TlvItem item = {
            .tag = tag,
            .tagLen = tagLen,
            .value = value,
            .valueLen = valueLen
        };

        if (cb(&item, user) != 0)
            return -1;
    }

    return 0;
}

OOP_CTOR(TLV) {
    self->decode = decode;
    self->encode = encode;
}

TLV *tlv() {
    CALL_ONCE(
        __tlv = GET_MEM(sizeof(__tlv));
        OOP_CALL_CTOR(TLV, __tlv);
    );
    return __tlv;
}