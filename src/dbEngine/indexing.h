#ifndef INDEXING_H_
#define INDEXING_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* =========================================================
 * CONFIG
 * ========================================================= */

#define MAX_GROUPS        8
#define MAX_BUCKET_SIZE   256

/* =========================================================
 * KEY EXTRACTION CALLBACK
 * ========================================================= */

typedef uint32_t (*IndexKeyFn)(const uint8_t *payload, uint32_t size);

/* =========================================================
 * PRIMARY INDEX
 * ========================================================= */

typedef struct {
    uint32_t key;
    uint32_t offset;
} IndexEntry;

typedef struct {
    IndexEntry *entries;
    uint32_t size;
    uint32_t capacity;
} PrimaryIndex;

/* =========================================================
 * SECONDARY INDEX
 * ========================================================= */

typedef struct {
    uint32_t offsets[MAX_BUCKET_SIZE];
    uint16_t count;
} GroupBucket;

typedef struct {
    GroupBucket buckets[MAX_GROUPS];
} GroupIndex;

/* =========================================================
 * INDEX CONTEXT
 * ========================================================= */

typedef struct {
    PrimaryIndex primary;
    GroupIndex   group;
    uint32_t     last_valid_offset;
    IndexKeyFn   key_fn;
} IndexDB;

int index_get(IndexDB *idx, uint32_t key, uint32_t *offset);

GroupBucket* index_get_group(IndexDB *idx, uint8_t group);

void index_add(IndexDB *idx, uint32_t key, uint32_t offset);

void index_init(IndexDB *idx,
                IndexEntry *buffer,
                uint32_t capacity,
                IndexKeyFn key_fn);

#endif