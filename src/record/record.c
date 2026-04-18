#include "record.h"
#include "medb.h"
#include "../dev/dev.h"
#include "eventloop.h"
#include "logger.h"
#include "file/file.h"
#include "utility/arith.h"

Record *__record;

#define RECORD_FILE "/mtd0/txn_rec"
#define MAX_RECORDS 6000

/* ================= SCHEMA ================= */

static MedbField recordFields[] = {
    { "status",  offsetof(TxnRecord, status),  1, MEDB_U8 },
    { "type",    offsetof(TxnRecord, type),    1, MEDB_U8 },
    { "time",    offsetof(TxnRecord, time),    6, MEDB_BLOB },
    { "voucher", offsetof(TxnRecord, voucher), 3, MEDB_BLOB },
};

static MedbTable recordTable = {
    .tableName = "txn",
    .fields = {0}, // will be fixed in init
    .fieldCount = 4,
    .rowSize = sizeof(TxnRecord),
    .maxRows = MAX_RECORDS,
    .writePolicy = MEDB_WRITE_CIRCULAR_OVERWRITE
};

/* ================= STORAGE ADAPTER ================= */

static bool rec_read(uint32_t index, void *out)
{
    uint32_t len = sizeof(TxnRecord);

    return OOP_CALL(file(), read,
        RECORD_FILE,
        out,
        index * sizeof(TxnRecord),
        &len) == FILE_ERR_OK;
}

static bool rec_write(uint32_t index, const void *in)
{
    return OOP_CALL(file(), insert,
        RECORD_FILE,
        (uint8_t*)in,
        index * sizeof(TxnRecord),
        sizeof(TxnRecord)) == FILE_ERR_OK;
}

static MedbStorage storage = {
    .read = rec_read,
    .write = rec_write
};

/* ================= DB INSTANCE ================= */

static Medb db;

/* ================= INIT ================= */

static void recordInit(void)
{
    /* FIX 1: attach schema properly */
    recordTable.fields[0] = recordFields[0];
    recordTable.fields[1] = recordFields[1];
    recordTable.fields[2] = recordFields[2];
    recordTable.fields[3] = recordFields[3];

    db.schema = &recordTable;
    db.storage = storage;

    /* FIX 2: initialize runtime state (CRITICAL) */
    db.rt.start_pos = 0;
    db.rt.write_pos = 0;
    db.rt.count = 0;

    /*
     * OPTIONAL (recommended):
     * here you should load persisted metadata from file
     * if you implement crash recovery later
     */
}

/* ================= INSERT ================= */

static int32_t recordAdd(TxnRecord *r)
{
    int idx = medb_insert(&db, r);

    return (idx >= 0) ? ERR_OK : ERR_NOK;
}

/* ================= FIXED: NO FILE-SIZE LOGIC ================= */

static uint32_t recordCount(void)
{
    /* IMPORTANT:
       DO NOT use file size anymore
       MEDB is logical storage now
    */
    return db.rt.count;
}

/* ================= LAST RECORD ================= */

static int32_t recordGetLastIndex(void)
{
    if (db.rt.count == 0)
        return ERR_NOK;

    /* logical last index (NOT physical file index) */
    uint32_t lastLogical = db.rt.count - 1;

    return (int32_t)((db.rt.start_pos + lastLogical) % db.schema->maxRows);
}

/* ================= OOP WRAPPER ================= */

OOP_CTOR(Record) {
    self->init = recordInit;
    self->add = recordAdd;
    self->getLastIdx = recordGetLastIndex;
}

Record *record()
{
    CALL_ONCE(
        OOP_CALL_CTOR(Record, __record);
    );
    return __record;
}