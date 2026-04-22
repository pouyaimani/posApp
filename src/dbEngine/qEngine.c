/* =========================================================
 * Layer 4 - Query Engine (Embedded DB)
 * =========================================================
 * Design:
 * - Index-first execution
 * - WAL fallback scan only when required
 * - No dynamic query planning (deterministic)
 * - Works over Layer 2 (WAL) + Layer 3 (Index)
 * ========================================================= */

#include "qEngine.h"

/* =========================================================
 * QUERY EXECUTION
 * ========================================================= */

int qe_execute(QueryEngine *qe,
               QueryPlan *plan,
               QueryResult *res)
{
    res->count = 0;

    uint32_t offset;

    /* 1. INDEX PATH */
    if (plan->use_key)
    {
        if (index_get(qe->index, plan->key, &offset) == 0)
        {
            res->offsets[res->count++] = offset;
            return 0;
        }

        if (!plan->fallback_scan)
            return -1;
    }

    /* 2. FILTERED INDEX PATH */
    if (plan->use_status)
    {
        GroupBucket *b = index_get_group(qe->index, plan->status);

        for (uint32_t i = 0; i < b->count; i++)
            res->offsets[res->count++] = b->offsets[i];

        return 0;
    }

    /* 3. WAL SCAN */
    if (plan->fallback_scan)
    {
        WalIterator it;
        wal_iter_init(&it, qe->wal, qe->io);

        WalIterRecord rec;

        while (wal_iter_next(&it, &rec))
        {
            res->offsets[res->count++] = rec.offset;
        }
    }

    return 0;
}
