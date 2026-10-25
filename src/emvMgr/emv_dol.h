#ifndef EMV_DOL_H
#define EMV_DOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t       tag;
    const uint8_t* value;
    size_t         length;
} EmvTlv;

/**
 * Supplies one data element requested by a DOL.
 *
 * The provider must write exactly requiredLength bytes.
 * Return false if the value cannot be supplied.
 */
typedef bool (*EmvDolValueFn)(void* context, uint32_t tag, uint8_t* output,
                              size_t requiredLength);

/**
 * Read the next BER-TLV element.
 *
 * Returns false for malformed data.
 * End of input is indicated by *offset == size.
 */
bool emvTlvNext(const uint8_t* data, size_t size, size_t* offset,
                EmvTlv* result);

/**
 * Build concatenated DOL values in the card-requested order.
 *
 * A DOL contains tag-length pairs, not full TLV objects.
 * Returns false if malformed, unsupported or too large.
 */
bool emvDolBuild(const uint8_t* dol, size_t dolLength, EmvDolValueFn provider,
                 void* providerContext, uint8_t* output, size_t capacity,
                 size_t* outputLength);

#endif