#include "emv_dol.h"

#include <string.h>

/**
 * Decode an EMV BER tag (1-3 bytes).
 */
static bool readTag(const uint8_t* data, size_t size, size_t* offset,
                    uint32_t* tag) {
    uint8_t      first;
    uint32_t     value;
    unsigned int count = 1u;

    if (!data || !offset || !tag || *offset >= size)
        return false;

    first = data[(*offset)++];

    if (first == 0x00u || first == 0xFFu)
        return false;

    value = first;

    if ((first & 0x1Fu) == 0x1Fu) {
        uint8_t next;

        do {
            if (*offset >= size || count >= 3u)
                return false;

            next = data[(*offset)++];

            value = (value << 8u) | next;
            ++count;

        } while ((next & 0x80u) != 0u);
    }

    *tag = value;
    return true;
}

/**
 * Decode a definite BER-TLV length.
 *
 * Supports short form and 0x81/0x82 long forms.
 */
static bool readLength(const uint8_t* data, size_t size, size_t* offset,
                       size_t* length) {
    uint8_t first;
    size_t  count;
    size_t  value = 0u;

    if (!data || !offset || !length || *offset >= size)
        return false;

    first = data[(*offset)++];

    if ((first & 0x80u) == 0u) {
        *length = first;
        return true;
    }

    count = first & 0x7Fu;

    if (count == 0u || count > 2u || count > size - *offset)
        return false;

    for (size_t i = 0u; i < count; ++i)
        value = (value << 8u) | data[(*offset)++];

    /* Reject non-minimal BER length encoding. */
    if (value < 128u || (count == 2u && value <= 255u))
        return false;

    *length = value;
    return true;
}

bool emvTlvNext(const uint8_t* data, size_t size, size_t* offset,
                EmvTlv* result) {
    uint32_t tag;
    size_t   length;

    if (!data || !offset || !result || *offset > size)
        return false;

    if (!readTag(data, size, offset, &tag))
        return false;

    if (!readLength(data, size, offset, &length))
        return false;

    if (length > size - *offset)
        return false;

    result->tag    = tag;
    result->value  = data + *offset;
    result->length = length;

    *offset += length;

    return true;
}

bool emvDolBuild(const uint8_t* dol, size_t dolLength, EmvDolValueFn provider,
                 void* providerContext, uint8_t* output, size_t capacity,
                 size_t* outputLength) {
    size_t offset  = 0u;
    size_t written = 0u;

    if (!output || !outputLength || !provider || (dolLength != 0u && !dol))
        return false;

    *outputLength = 0u;

    while (offset < dolLength) {
        uint32_t tag;
        size_t   requiredLength;

        if (!readTag(dol, dolLength, &offset, &tag))
            goto failed;

        if (offset >= dolLength)
            goto failed;

        /* DOL lengths are encoded in one byte. */
        requiredLength = dol[offset++];

        if (requiredLength > capacity - written)
            goto failed;

        /*
         * A zero-length element does not require a value.
         * The provider is invoked only for nonzero lengths.
         */
        if (requiredLength != 0u) {
            memset(output + written, 0, requiredLength);

            if (!provider(providerContext, tag, output + written,
                          requiredLength))
                goto failed;
        }

        written += requiredLength;
    }

    *outputLength = written;
    return true;

failed:
    memset(output, 0, capacity);
    return false;
}