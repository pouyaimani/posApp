#include "record.h"
#include "embedDB/embedDB.h"
#include "embedDB/embedDBsetup.h"
#include "embedDB/embedDB_mem.h"
#include "logger.h"
#include "file/file.h"

Record *__record;

#define TRANS_RECORD_PATH   "/mtd0/txn_records"

#define MAX_RECORDS 6000

#define WRITE_LOG "written.log"
#define READ_LOG  "read.log"

// ---------- Transaction Data ----------
typedef struct {
    int32_t amount;
    int32_t timestamp;
    int32_t type;
} TxnData;

// ---------- Global ----------
embedDBState *state;

// ---------- Logging ----------
void log_write(uint32_t idx, uint32_t key, TxnData *d) {
    LOG_DEBUG("W idx=%03u key=%03u amount=%d ts=%d type=%d\n",
            idx, key, d->amount, d->timestamp, d->type);
}

void log_read(uint32_t idx, uint32_t key, int found, TxnData *d) {
    if (found) {
        LOG_DEBUG(
                "R idx=%03u key=%03u FOUND amount=%d ts=%d type=%d\n",
                idx, key, d->amount, d->timestamp, d->type);
    } else {
        LOG_DEBUG(
                "R idx=%03u key=%03u NOT_FOUND\n",
                idx, key);
    }
}

// ---------- TearDown ----------
void tearDownEmbedDB() {
    embedDBClose(state);
    tearDownFile(state->dataFile);

    EMDB_MEM_FREE(state->buffer);
    EMDB_MEM_FREE(state->fileInterface);
    EMDB_MEM_FREE(state);
}

// ---------- Unity hooks ----------
void setUp(void) {}
void tearDown(void) {}

// ============================================================
// 🧪 TEST: Circular Overwrite + Logging
// ============================================================
void test_circular_overwrite_with_logs() {
    state = (embedDBState *)EMDB_MEM_ALLOC(sizeof(embedDBState));
    // setupEmbedDB(state, TRANS_RECORD_PATH, 10000,
    //                               sizeof(TxnData), sizeof(uint32_t));

    uint32_t totalInsert = 150; // force overwrite
    // ---------- WRITE ----------
    for (uint32_t i = 0; i < totalInsert; i++) {
        uint32_t key = i;

        TxnData data;
        data.amount = i * 10;
        data.timestamp = i + 1000;
        data.type = i % 2;

        log_write(i, key, &data);

        int8_t res = embedDBPut(state, &key, &data);
    }
LOG_DEBUG("---------------------");
    embedDBFlush(state);

    // ---------- Compute capacity ----------
    uint32_t capacity =
        state->numDataPages * state->maxRecordsPerPage;

    uint32_t minValidKey =
        (totalInsert > capacity) ? (totalInsert - capacity) : 0;

    // ---------- READ ----------
// ---------- ITERATOR READ + LOG ----------
    embedDBIterator it;
    memset(&it, 0, sizeof(it));

    embedDBInitIterator(state, &it);
    LOG_DEBUG("---------------------");

    uint8_t *seen = EMDB_MEM_ALLOC(totalInsert);
    memset(seen, 0 , totalInsert);
    // TEST_ASSERT_NOT_NULL(seen);

    uint32_t key;
    TxnData data;
    uint32_t read_idx = 0;

    while (embedDBNext(state, &it, &key, &data)) {

        // mark seen
        if (key < totalInsert)
            seen[key] = 1;

        // -------- LOG READ --------
        log_read(read_idx,
                key,1,
                &data);

        read_idx++;

        // optional: data integrity check
        // TEST_ASSERT_EQUAL_INT32(key * 10, data.amount);
        // TEST_ASSERT_EQUAL_INT32(key + 1000, data.timestamp);
        // TEST_ASSERT_EQUAL_INT32(key % 2, data.type);
    }
LOG_DEBUG("---------------------");
    embedDBCloseIterator(&it);
LOG_DEBUG("---------------------");
    tearDownEmbedDB();
}

// ============================================================
// 🚀 Runner
// ============================================================
int doTest(void) {
    test_circular_overwrite_with_logs();
}

/* ================= OOP ================= */

OOP_CTOR(Record) {
    // self->init = recordInit;
    // self->add = recordAdd;
    // self->getLastIdx = recordGetLastIndex;
    // // self->count = recordCount;
}

Record *record()
{
    CALL_ONCE(
        OOP_CALL_CTOR(Record, __record);
    );
    return __record;
}