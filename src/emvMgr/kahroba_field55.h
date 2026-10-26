#ifndef KAHROBA_FIELD55_H
#define KAHROBA_FIELD55_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KAHROBA_FIELD55_MAX_LEN 512u

typedef struct {
    uint8_t* data;
    size_t   capacity;
    size_t   length;
} KahrobaField55Writer;

/**
 * Initialize a binary BER-TLV writer.
 */
bool kahrobaField55Init(KahrobaField55Writer* writer, uint8_t* buffer,
                        size_t capacity);

/**
 * Append one BER-TLV item.
 *
 * Supports one- or two-byte tags and value lengths
 * between 1 and 127 bytes.
 *
 * Returns false if arguments are invalid or the
 * destination buffer has insufficient capacity.
 *
 * On failure, the writer's length is unchanged.
 */
bool kahrobaField55Append(KahrobaField55Writer* writer, uint16_t tag,
                          const uint8_t* value, size_t valueLength);

/**
 * @brief Trace Field 55 writer data in hexadecimal.
 *
 * DEBUG ONLY:
 * Use for non-sensitive test data such as base tags.
 * Never log a complete production Field 55 containing
 * PAN (5A), Track 2 (57), or other sensitive card data.
 */
void kahrobaTraceField55(const KahrobaField55Writer* writer);

#endif