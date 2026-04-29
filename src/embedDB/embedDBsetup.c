#include "embedDBsetup.h"
#include "embedDB_mem.h"
#include "EmbedDB_Utility/embedDBUtility.h"
#include "logger.h"

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
                                  uint32_t dataSize, uint32_t keySize)
{
    EmbedDBTune cfg = {0};

    // 1. Pick page size
    cfg.pageSize = pick_page_size(dataSize);

    // 2. Compute records per page
    cfg.recordsPerPage =
        (cfg.pageSize - HEADER_SIZE) / (dataSize + keySize);

    // 3. Compute number of pages
    cfg.numDataPages = maxStorageBytes / cfg.pageSize;

    // 4. Set erase block size
    cfg.eraseSizeInPages = 1;

    // 5. Enforce minimum pages constraint
    if (cfg.numDataPages < 2 * cfg.eraseSizeInPages)
    {
        cfg.numDataPages = 2 * cfg.eraseSizeInPages;
    }

    return cfg;
}

void setupEmbedDB(embedDBState *state, const char *dbName, uint32_t maxStorageBytes,
                  uint32_t dataSize, uint32_t keySize)
{

    EmbedDBTune tune = embeddb_calc_config(maxStorageBytes, dataSize, keySize);
    LOG_DEBUG("tune.pageSize = %d", tune.pageSize);
    LOG_DEBUG("tune.numDataPages = %d", tune.numDataPages);
    LOG_DEBUG("tune.eraseSizeInPages = %d", tune.eraseSizeInPages);
    LOG_DEBUG("tune.recordsPerPage = %d", tune.recordsPerPage);

    state->keySize = keySize;
    state->dataSize = dataSize;

    state->pageSize = tune.pageSize;
    // state->maxRecordsPerPage = tune.recordsPerPage;
    state->bufferSizeInBlocks = 2;
    state->buffer = EMDB_MEM_ALLOC(state->bufferSizeInBlocks * state->pageSize);

    state->numSplinePoints = 8;
    state->numDataPages = tune.numDataPages;
    state->numIndexPages = 0;

    state->parameters = EMBEDDB_RESET_DATA;

    state->eraseSizeInPages = tune.eraseSizeInPages;
    state->rules = NULL;
    state->numRules = 0;

    state->compareKey = int32Comparator;
    state->compareData = NULL;

    state->fileInterface = getFileInterface();
    state->dataFile = setupFile(dbName);

    int8_t res = embedDBInit(state, 1);
    return res;
}