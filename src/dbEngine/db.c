#include "db.h"

int db_insert(GenericDB *db, DbDoc *doc)
{
    /* 1. append to WAL (Layer 2) */
    int ret = wal_append((WalLog*)db->wal,
                            doc->data,
                            doc->size,
                            1,
                            db->io);

    if (ret != 0)
        return -1;

    /* 2. compute key for indexing */
    uint32_t offset = db->wal->write_offset;

    uint32_t key = db->key_fn(doc->data, doc->size);

    /* 3. update index */
    index_add(db->index, key, offset);

    return 0;
}

int db_query(GenericDB *db,
             QueryPlan *plan,
             QueryResult *res)
{
    QueryEngine qe = {
        .index = db->index,
        .wal = db->wal,
        .io = db->io
    };

    return qe_execute(&qe, plan, res);
}