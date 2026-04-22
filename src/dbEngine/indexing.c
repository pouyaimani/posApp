/* =========================================================
 * Layer 3 - Generic Indexing System (WAL Iterator-based)
 * =========================================================
 * Design:
 * - Fully generic payload (no domain dependency)
 * - Uses WAL ITERATOR (no manual WAL parsing in index layer)
 * - Primary Index: key -> WAL offset
 * - Secondary Index: grouping buckets
 * - Fully rebuildable from WAL stream
 * - Tolerant to corruption via iterator resync
 * ========================================================= */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "wal/wal.h"
#include "indexing.h"

/* =========================================================
 * INTERNAL RESET
 * ========================================================= */

static void index_reset(IndexDB *idx)
{
    idx->primary.size = 0;
    idx->last_valid_offset = 0;

    for (int i = 0; i < MAX_GROUPS; i++)
        idx->group.buckets[i].count = 0;
}

/* =========================================================
 * LOOKUP
 * ========================================================= */

static int index_find(IndexDB *idx, uint32_t key, uint32_t *offset)
{
    int l = 0;
    int r = (int)idx->primary.size - 1;

    while (l <= r)
    {
        int m = (l + r) / 2;
        uint32_t mid = idx->primary.entries[m].key;

        if (mid == key)
        {
            *offset = idx->primary.entries[m].offset;
            return 0;
        }
        else if (mid < key)
            l = m + 1;
        else
            r = m - 1;
    }

    return -1;
}

/* =========================================================
 * INDEX INSERT
 * ========================================================= */

void index_add(IndexDB *idx, uint32_t key, uint32_t offset)
{
    PrimaryIndex *p = &idx->primary;

    if (p->size < p->capacity)
    {
        p->entries[p->size].key = key;
        p->entries[p->size].offset = offset;
        p->size++;
    }
}

/* =========================================================
 * REBUILD FROM WAL ITERATOR
 * ========================================================= */

void index_rebuild(IndexDB *idx, WalLog *log, WalIoBuffer *io)
{
    index_reset(idx);

    WalIterator it;
    wal_iter_init(&it, log, io);

    WalIterRecord rec;

    while (wal_iter_next(&it, &rec))
    {
        uint32_t key = idx->key_fn(rec.payload, rec.size);

        index_add(idx, key, rec.offset);

        idx->last_valid_offset = rec.offset;
    }
}

/* =========================================================
 * PUBLIC API
 * ========================================================= */

int index_get(IndexDB *idx, uint32_t key, uint32_t *offset)
{
    return index_find(idx, key, offset);
}

GroupBucket* index_get_group(IndexDB *idx, uint8_t group)
{
    if (group >= MAX_GROUPS)
        return NULL;

    return &idx->group.buckets[group];
}

/* =========================================================
 * INIT
 * ========================================================= */

void index_init(IndexDB *idx,
                IndexEntry *buffer,
                uint32_t capacity,
                IndexKeyFn key_fn)
{
    idx->primary.entries = buffer;
    idx->primary.capacity = capacity;
    idx->primary.size = 0;

    idx->key_fn = key_fn;

    index_reset(idx);
}
