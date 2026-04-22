#include "record.h"
#include "medb.h"
#include "file/file.h"

Record *__record;

#define RECORD_FILE "/mtd0/txn_rec"
#define MAX_RECORDS 6000

/* ================= SCHEMA ================= */

static const MedbField recordFields[] = {
    { "status",  offsetof(TxnRecord, status),  1, MEDB_U8 },
    { "type",    offsetof(TxnRecord, type),    1, MEDB_U8 },
    { "time",    offsetof(TxnRecord, time),    6, MEDB_BLOB },
    { "voucher", offsetof(TxnRecord, voucher), 3, MEDB_BLOB },
};

static const MedbTable recordTable = {
    .tableName = "txn",
    .fields = recordFields,
    .fieldCount = 4,
    .rowSize = sizeof(TxnRecord),
    .maxRows = MAX_RECORDS,
    .writePolicy = MEDB_WRITE_CIRCULAR_OVERWRITE
};

/* ================= STORAGE ================= */

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

static Medb db = {
    .schema = &recordTable,
    .storage = {
        .read = rec_read,
        .write = rec_write
    }
};

/* ================= INIT ================= */

static void recordInit(void)
{
    db.rt.start_pos = 0;
    db.rt.write_pos = 0;
    db.rt.count = 0;

    /* TODO: load from persistent header in future */
}

/* ================= INSERT ================= */

static int32_t recordAdd(TxnRecord *r)
{
    uint32_t idx;

    int rc = medb_insert(&db, r, &idx);
    return (rc == MEDB_OK) ? ERR_OK : ERR_NOK;
}

/* ================= INFO ================= */

static uint32_t recordCount(void)
{
    return db.rt.count;
}

static int32_t recordGetLastIndex(void)
{
    if (db.rt.count == 0)
        return ERR_NOK;

    uint32_t lastLogical = db.rt.count - 1;
    return (db.rt.start_pos + lastLogical) % db.schema->maxRows;
}

/* ================= OOP ================= */

OOP_CTOR(Record) {
    self->init = recordInit;
    self->add = recordAdd;
    self->getLastIdx = recordGetLastIndex;
    // self->count = recordCount;
}

Record *record()
{
    CALL_ONCE(
        OOP_CALL_CTOR(Record, __record);
    );
    return __record;
}