#include "kahroba_field55.h"
#include "logger.h"
#include "emv_kahroba.h"

#include <stdio.h>

/**
 * @brief Trace Field 55 writer data in hexadecimal.
 *
 * DEBUG ONLY:
 * Use for non-sensitive test data such as base tags.
 * Never log a complete production Field 55 containing
 * PAN (5A), Track 2 (57), or other sensitive card data.
 */
void kahrobaTraceField55(const KahrobaField55Writer* writer) {
    if (!writer || !writer->data || writer->length > writer->capacity) {
        LOG_TRACE("Kahroba: invalid Field55 writer");
        return;
    }

    LOG_TRACE("Kahroba: Field55 length=%lu", (unsigned long)writer->length);

    for (size_t i = 0u; i < writer->length; i += 16u) {
        char   line[16u * 3u + 1u] = {0};
        size_t pos                 = 0u;

        for (size_t j = i; j < writer->length && j < i + 16u; ++j) {

            int n = snprintf(line + pos, sizeof(line) - pos, "%02X ",
                             (unsigned int)writer->data[j]);

            if (n < 0 || (size_t)n >= sizeof(line) - pos)
                break;

            pos += (size_t)n;
        }

        LOG_TRACE("Kahroba: Field55 offset=%lu data=%s", (unsigned long)i,
                  line);
    }
}

bool kahrobaField55Init(KahrobaField55Writer* writer, uint8_t* buffer,
                        size_t capacity) {
    if (writer == NULL || buffer == NULL || capacity == 0u)
        return false;

    writer->data     = buffer;
    writer->capacity = capacity;
    writer->length   = 0u;

    return true;
}

bool kahrobaField55Append(KahrobaField55Writer* writer, uint16_t tag,
                          const uint8_t* value, size_t valueLength) {
    size_t tagLength;
    size_t required;
    size_t offset;

    if (writer == NULL || writer->data == NULL || value == NULL || tag == 0u ||
        valueLength == 0u || valueLength > 127u)
        return false;

    tagLength = (tag > 0xFFu) ? 2u : 1u;
    required  = tagLength + 1u + valueLength;

    /* Validate BER tag encoding for one- or two-byte tags. */
    if (tagLength == 1u) {
        if ((tag & 0x1Fu) == 0x1Fu)
            return false;
    } else {
        if (((tag >> 8u) & 0x1Fu) != 0x1Fu || (tag & 0x80u) != 0u)
            return false;
    }

    /* Prevent buffer overflow. */
    if (writer->length > writer->capacity ||
        required > writer->capacity - writer->length)
        return false;

    offset = writer->length;

    /* Encode tag. */
    if (tagLength == 2u)
        writer->data[offset++] = (uint8_t)(tag >> 8u);

    writer->data[offset++] = (uint8_t)tag;

    /* Encode short-form BER length. */
    writer->data[offset++] = (uint8_t)valueLength;

    /* Encode value. */
    for (size_t i = 0u; i < valueLength; ++i)
        writer->data[offset++] = value[i];

    writer->length = offset;

    return true;
}