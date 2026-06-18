#include "tlv.h"
#include "sys/sys.h"
#include "logger.h"
#include "error.h"

static TLV* __tlv;

static TlvError_t encode(uint8_t* out, uint16_t outSize, const uint8_t* tag,
                         uint8_t tagLen, const void* value, uint16_t valueLen,
                         uint16_t* encodedLen) {
    RETURN_VALUE_IF_NULL(out, ;, TLV_ERR_INVALID_PARAM);
    RETURN_VALUE_IF_NULL(tag, ;, TLV_ERR_INVALID_PARAM);
    RETURN_VALUE_IF_NULL(value, ;, TLV_ERR_INVALID_PARAM);
    RETURN_VALUE_IF_NULL(encodedLen, ;, TLV_ERR_INVALID_PARAM);
    if ((!value && valueLen > 0)) {
        LOG_DEBUG("tlv encode failed. invalid valueLen = %d.", valueLen);
        return TLV_ERR_INVALID_PARAM;
    }

    uint16_t offset = 0;

    // --- TAG ---
    if (offset + 1 + tagLen > outSize) {
        return TLV_ERR_BUFFER_TOO_SMALL;
    }

    out[offset++] = tagLen;
    memcpy(out + offset, tag, tagLen);
    offset += tagLen;

    // --- LENGTH ---
    if (valueLen <= 127) {
        if (offset + 1 > outSize)
            return TLV_ERR_BUFFER_TOO_SMALL;
        ;
        out[offset++] = (uint8_t)valueLen;
    } else if (valueLen <= 255) {
        if (offset + 2 > outSize)
            return TLV_ERR_BUFFER_TOO_SMALL;
        out[offset++] = 0x81;
        out[offset++] = (uint8_t)valueLen;
    } else {
        if (offset + 3 > outSize)
            return TLV_ERR_BUFFER_TOO_SMALL;
        out[offset++] = 0x82;
        out[offset++] = (uint8_t)(valueLen >> 8);
        out[offset++] = (uint8_t)(valueLen & 0xFF);
    }

    // --- VALUE ---
    if (valueLen > 0) {
        if (offset + valueLen > outSize)
            return TLV_ERR_BUFFER_TOO_SMALL;

        memcpy(out + offset, value, valueLen);
        offset += valueLen;
    }

    *encodedLen = offset;
    return TLV_OK;
}

static TlvError_t decode(const uint8_t* data, uint16_t len, TlvCallback cb,
                         void* user) {
    if (!data || !cb)
        return TLV_ERR_INVALID_PARAM;

    uint16_t offset = 0;

    while (offset < len) {
        // --- TAG ---
        if (offset + 1 > len)
            return TLV_ERR_TAG_OVERFLOW;

        uint8_t tagLen = data[offset++];

        if (tagLen == 0)
            return TLV_ERR_TAG_LENGTH_INVALID;
        if (offset + tagLen > len) {
            return TLV_ERR_TAG_OVERFLOW;
        }

        const uint8_t* tag = &data[offset];
        offset += tagLen;

        // --- LENGTH ---
        if (offset >= len)
            return TLV_ERR_LENGTH_MISSING;

        uint16_t valueLen = 0;
        uint8_t  lenByte  = data[offset++];

        if (lenByte <= 127) {
            valueLen = lenByte;
        } else {
            uint8_t lenBytes = lenByte & 0x7F;

            if (lenBytes == 0 || lenBytes > 2)
                return TLV_ERR_LENGTH_INVALID; // we support up to 2 bytes
                                               // length

            if (offset + lenBytes > len)
                return TLV_ERR_LENGTH_OVERFLOW;

            for (uint8_t i = 0; i < lenBytes; i++) {
                valueLen = (valueLen << 8) | data[offset++];
            }
        }

        // --- VALUE ---
        if (offset + valueLen > len)
            return TLV_ERR_VALUE_OVERFLOW;

        const uint8_t* value = &data[offset];
        offset += valueLen;

        TlvItem item = {
            .tag = tag, .tagLen = tagLen, .value = value, .valueLen = valueLen};

        if (cb(&item, user) != 0)
            return TLV_ERR_CALLBACK_FAILED;
    }

    return TLV_OK;
}

OOP_CTOR(TLV) {
    self->decode = decode;
    self->encode = encode;
}

TLV* tlv() {
    CALL_ONCE(__tlv = MEM_ALLOC(sizeof(__tlv)); OOP_CALL_CTOR(TLV, __tlv););
    return __tlv;
}