#include "embedDBsetup.h"
#include "embedDB_mem.h"
#include "EmbedDB_Utility/embedDBUtility.h"

typedef struct {
    uint32_t pageSize;
    uint32_t numDataPages;
    uint32_t eraseSizeInPages;
    uint32_t recordsPerPage;
} EmbedDBTune;

#define HEADER_SIZE 6

static uint32_t pick_page_size(uint32_t recordSize) {
    uint32_t candidates[] = {256, 512, 1024};

    for (int i = 0; i < 3; i++) {
        uint32_t usable = candidates[i] - HEADER_SIZE;
        uint32_t rpp = usable / recordSize;

        if (EMBEDDB_IS_GOOD_RPP(rpp))
            return candidates[i];
    }

    return 1024; // fallback
}

EmbedDBTune embeddb_calc_config(uint32_t maxStorageBytes,
                                  uint32_t recordSize)
{
    EmbedDBTune cfg = {0};

    // 1. Pick page size
    cfg.pageSize = pick_page_size(recordSize);

    // 2. Compute records per page
    cfg.recordsPerPage =
        (cfg.pageSize - HEADER_SIZE) / recordSize;

    // 3. Compute number of pages
    cfg.numDataPages = maxStorageBytes / cfg.pageSize;

    // 4. Set erase block size
    cfg.eraseSizeInPages = 4;

    // 5. Enforce minimum pages constraint
    if (cfg.numDataPages < 2 * cfg.eraseSizeInPages) {
        cfg.numDataPages = 2 * cfg.eraseSizeInPages;
    }

    return cfg;
}

void setupEmbedDB(embedDBState *state, const char *dbName, uint32_t maxStorageBytes,
                                  uint32_t recordSize, uint32_t keySize) {

    EmbedDBTune tune = embeddb_calc_config(maxStorageBytes, recordSize);

    state->keySize = keySize;
    state->dataSize = recordSize;

    state->pageSize = 512;
    // state->maxRecordsPerPage = tune.recordsPerPage;
    state->bufferSizeInBlocks = 2;
    state->buffer = EMDB_MEM_ALLOC(state->bufferSizeInBlocks * state->pageSize);

    state->numSplinePoints = 8;
    state->numDataPages = 4;
    state->numIndexPages = 0;

    state->parameters = EMBEDDB_RESET_DATA;

    state->eraseSizeInPages = 1;
    state->rules = NULL;
    state->numRules = 0;

    state->compareKey = int32Comparator;
    state->compareData = NULL;

    state->fileInterface = getFileInterface();
    state->dataFile = setupFile(dbName);

    int8_t res = embedDBInit(state, 1);
    return res;
}