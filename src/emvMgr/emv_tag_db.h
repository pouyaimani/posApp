#ifndef EMV_TAG_DB_H
#define EMV_TAG_DB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fixed allocation: tune to the memory budget and card profiles on the device. */
#ifndef EMV_TAG_DB_MAX_ITEMS
#define EMV_TAG_DB_MAX_ITEMS 48u
#endif
#ifndef EMV_TAG_DB_ARENA_SIZE
#define EMV_TAG_DB_ARENA_SIZE 1536u
#endif
#if EMV_TAG_DB_ARENA_SIZE > 65535u
#error "EmvTagEntry offset is uint16_t; reduce EMV_TAG_DB_ARENA_SIZE"
#endif
#ifndef EMV_TLV_MAX_DEPTH
#define EMV_TLV_MAX_DEPTH 8u
#endif

typedef enum {
    EMV_OK = 0,
    EMV_ERR_ARGUMENT,
    EMV_ERR_STATE,
    EMV_ERR_CAPACITY,
    EMV_ERR_TLV,
    EMV_ERR_MISSING_TAG,
    EMV_ERR_PROTOCOL,
    EMV_ERR_TRANSPORT,
    EMV_ERR_TIMEOUT,
    EMV_ERR_UNSUPPORTED,
    EMV_ERR_RANDOM
} EmvError;

/* Tag identifiers are ordinary big-endian tag bytes (e.g., 0x9F26). */
typedef uint32_t EmvTag;

typedef struct {
    EmvTag tag;       /* Parsed BER-TLV tag number/identifier. */
    uint16_t offset;  /* Offset into bytes[] owned by this database. */
    uint16_t length;  /* Number of bytes in this tag value. */
} EmvTagEntry;

typedef struct {
    EmvTagEntry entries[EMV_TAG_DB_MAX_ITEMS]; /* Indexed primitive tags. */
    uint8_t bytes[EMV_TAG_DB_ARENA_SIZE];     /* Owned bytes, no external pointers. */
    size_t used;                              /* Arena bytes already consumed. */
    size_t count;                             /* Number of indexed entries. */
} EmvTagDb;

/** Reset metadata and stored data, including sensitive card data. */
void emvTagDbClear(EmvTagDb *db);

/** Store a copied value. Replacing same-length tags reuses existing space. */
EmvError emvTagDbSet(EmvTagDb *db, EmvTag tag,
                     const uint8_t *value, size_t value_len);

/** Fetch borrowed value; invalidated by clear/overwrite, not by other inserts. */
bool emvTagDbGet(const EmvTagDb *db, EmvTag tag,
                 const uint8_t **value, size_t *length);

/** Collect primitive tags from a strict, recursively validated BER-TLV. */
EmvError emvTlvCollect(EmvTagDb *db, const uint8_t *tlv, size_t len);

/**
 * Collect card TLV without permitting alteration of already stored tags.
 * Repeated identical primitive tags are accepted; conflicting values fail.
 * This prevents malicious card data replacing terminal amount, UN, etc.
 */
EmvError emvTlvCollectProtected(EmvTagDb *db, const uint8_t *tlv, size_t len);

/** Find an exact primitive tag/value anywhere in a BER-TLV tree. */
EmvError emvTlvContains(const uint8_t *tlv, size_t len, EmvTag tag,
                        const uint8_t *value, size_t value_len, bool *found);

#endif
