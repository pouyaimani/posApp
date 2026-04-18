#ifndef MEDB_H_
#define MEDB_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ================= CONFIG ================= */

#define MEDB_ERR_FULL -1
#define MEDB_ERR_INVALID -2

#ifndef MEDB_MAX_FIELDS
#define MEDB_MAX_FIELDS 16
#endif

#ifndef MEDB_MAX_CONDITIONS
#define MEDB_MAX_CONDITIONS 6
#endif

/* ================= WRITE POLICY ================= */

typedef enum {
    MEDB_WRITE_FAIL_IF_FULL = 0,
    MEDB_WRITE_CIRCULAR_OVERWRITE = 1
} MedbWritePolicy;

/* ================= FIELD TYPES ================= */

typedef enum {
    MEDB_U8,
    MEDB_U16,
    MEDB_U32,
    MEDB_BLOB
} MedbType;

/* ================= FIELD ================= */

typedef struct {
    const char *name;
    uint16_t offset;
    uint8_t size;
    MedbType type;
} MedbField;

/* ================= SCHEMA ================= */

typedef struct {
    const char *tableName;

    MedbField fields[MEDB_MAX_FIELDS];
    uint8_t fieldCount;

    uint16_t rowSize;
    uint32_t maxRows;

    MedbWritePolicy writePolicy;
} MedbTable;

/* ================= QUERY ================= */

typedef enum {
    MEDB_EQ,
    MEDB_NEQ,
    MEDB_LT,
    MEDB_LTE,
    MEDB_GT,
    MEDB_GTE,
    MEDB_BETWEEN
} MedbOp;

typedef struct {
    uint8_t fieldIndex;
    MedbOp op;

    union {
        uint32_t num;

        struct {
            uint8_t start[8];
            uint8_t end[8];
        } range;

    } value;

} MedbCondition;

typedef struct {
    MedbCondition conds[MEDB_MAX_CONDITIONS];
    uint8_t count;

    uint32_t limit;
    bool reverse;
} MedbQuery;

/* ================= STORAGE BACKEND ================= */

typedef struct {
    bool (*read)(uint32_t index, void *out);
    bool (*write)(uint32_t index, const void *in);
} MedbStorage;

/* ================= INTERNAL DB STATE ================= */

typedef struct {
    uint32_t start_pos;   // oldest record
    uint32_t write_pos;   // next write slot
    uint32_t count;       // valid records
} MedbRuntime;

/* ================= DB ================= */

typedef struct {
    MedbTable *schema;
    MedbStorage storage;
    MedbRuntime rt;
} Medb;

/* ================= HELPERS ================= */

static inline const uint8_t* medb_field_ptr(const void *row, uint16_t offset)
{
    return (const uint8_t*)row + offset;
}

/* ================= CONDITION EVAL ================= */

static inline bool medb_eval(
    const void *row,
    const MedbTable *table,
    const MedbCondition *c)
{
    if (c->fieldIndex >= table->fieldCount)
        return false;

    const MedbField *f = &table->fields[c->fieldIndex];
    const uint8_t *p = medb_field_ptr(row, f->offset);

    switch (f->type)
    {
        case MEDB_U8:
        case MEDB_U16:
        case MEDB_U32:
        {
            uint32_t v = 0;
            memcpy(&v, p, f->size);

            switch (c->op) {
                case MEDB_EQ:  return v == c->value.num;
                case MEDB_NEQ: return v != c->value.num;
                case MEDB_LT:  return v < c->value.num;
                case MEDB_LTE: return v <= c->value.num;
                case MEDB_GT:  return v > c->value.num;
                case MEDB_GTE: return v >= c->value.num;
                default: return false;
            }
        }

        case MEDB_BLOB:
            return memcmp(p, c->value.range.start, f->size) == 0;
    }

    return false;
}

static inline bool medb_match(
    const void *row,
    const MedbTable *t,
    const MedbQuery *q)
{
    for (uint8_t i = 0; i < q->count; i++) {
        if (!medb_eval(row, t, &q->conds[i]))
            return false;
    }
    return true;
}

/* ================= SELECT ================= */

static inline int medb_select(
    const Medb *db,
    const MedbQuery *query,
    void *rowBuf,
    uint32_t *outIdx,
    uint32_t maxOut,
    uint32_t *found)
{
    const uint32_t total = db->rt.count;
    uint32_t cnt = 0;

    if (total == 0) {
        *found = 0;
        return 0;
    }

    int i = query->reverse ? (int)total - 1 : 0;
    int end = query->reverse ? -1 : (int)total;
    int step = query->reverse ? -1 : 1;

    for (; i != end; i += step)
    {
        uint32_t phys = (db->rt.start_pos + i) % db->schema->maxRows;

        if (!db->storage.read(phys, rowBuf))
            continue;

        if (!medb_match(rowBuf, db->schema, query))
            continue;

        if (cnt < maxOut)
            outIdx[cnt] = phys;

        cnt++;

        if (query->limit && cnt >= query->limit)
            break;
    }

    *found = cnt;
    return 0;
}

/* ================= INSERT (FIXED CIRCULAR MODEL) ================= */

static inline int medb_insert(Medb *db, void *row)
{
    const uint32_t cap = db->schema->maxRows;

    /* CASE 1: still space available */
    if (db->rt.count < cap)
    {
        uint32_t idx = db->rt.write_pos;

        if (!db->storage.write(idx, row))
            return MEDB_ERR_INVALID;

        db->rt.write_pos = (db->rt.write_pos + 1) % cap;
        db->rt.count++;

        return idx;
    }

    /* CASE 2: FULL */
    if (db->schema->writePolicy == MEDB_WRITE_FAIL_IF_FULL)
        return MEDB_ERR_FULL;

    /* CASE 3: CIRCULAR OVERWRITE */
    uint32_t idx = db->rt.write_pos;

    if (!db->storage.write(idx, row))
        return MEDB_ERR_INVALID;

    db->rt.write_pos = (db->rt.write_pos + 1) % cap;
    db->rt.start_pos = (db->rt.start_pos + 1) % cap;

    return idx;
}

/* ================= QUERY API ================= */

static inline int medb_query(
    Medb *db,
    const MedbQuery *q,
    void *rowBuf,
    uint32_t *outIdx,
    uint32_t maxOut,
    uint32_t *found)
{
    return medb_select(db, q, rowBuf, outIdx, maxOut, found);
}

#endif