#include "embedDBsetup.h"
#include "logger.h"
#include <math.h>
#include "common.h"

#define safetyMargin 2

static inline void setBitmapSize(embedDBState* state, uint32_t numPages) {
    uint32_t buckets = numPages;

    if (buckets < 8) {
        buckets = 8; // minimum useful
    } else if (buckets < 16) {
        buckets = 16;
    } else {
        buckets = 64; // maximum realistic
    }
    state->bitmapSize = (buckets + 7) / 8; // round up

    if (state->bitmapSize == 1) {
        state->inBitmap             = inBitmapInt8;
        state->updateBitmap         = updateBitmapInt8;
        state->buildBitmapFromRange = buildBitmapInt8FromRange;
    } else if (state->bitmapSize == 2) {
        state->inBitmap             = inBitmapInt16;
        state->updateBitmap         = updateBitmapInt16;
        state->buildBitmapFromRange = buildBitmapInt16FromRange;
    } else if (state->bitmapSize == 8) {
        state->inBitmap             = inBitmapInt64;
        state->updateBitmap         = updateBitmapInt64;
        state->buildBitmapFromRange = buildBitmapInt64FromRange;
    }
}

static int getBufferCount(uint32_t parameters) {
    int count = 2;
    if (EMBEDDB_USING_INDEX(parameters))
        count += 2;
    if (EMBEDDB_USING_VDATA(parameters))
        count += 2;
    return count;
}
static inline uint32_t calcNumIndexPages(uint32_t numPages) {
    uint32_t n = numPages / 50; // ≈2%
    if (n < 4)
        n = 4; // minimum required for stability
    return n;
}

int8_t embedDBSetup(embedDBState* state, const char* dbPath,
                    const char* dbIndexPath, uint16_t keySize,
                    uint16_t dataSize, uint32_t pageSize, uint16_t pageNum,
                    uint16_t parameters) {
    if (!state)
        return -1;
    /* Basic configuration */
    state->keySize    = keySize;
    state->dataSize   = dataSize;
    state->parameters = parameters;

    state->recordSize = keySize + dataSize;
    /* Compute page size */
    state->pageSize = pageSize;

    /* Default erase block size */
    state->eraseSizeInPages = 1;
    state->numDataPages     = pageNum + safetyMargin;

    /* Minimum buffers */
    state->bufferSizeInBlocks = getBufferCount(state->parameters);
    size_t bufferSize         = state->bufferSizeInBlocks * state->pageSize;
    state->buffer             = EMDB_MEM_ALLOC(bufferSize);
    if (!state->buffer) {
#ifdef PRINT_ERRORS
        debug_log("ERROR: state->buffer memory allocation failed");
#endif
        return -1;
    }
    memset(state->buffer, 0, bufferSize);

    /* Default spline config */
    state->numSplinePoints = 8;

    // Function pointers that can compare two keys/data
    if (keySize == sizeof(uint32_t)) {
        state->compareKey = int32Comparator;
    } else if (keySize == sizeof(uint64_t)) {
        state->compareKey = int64Comparator;
    }
    // state->compareData = dataComparator;

    state->fileInterface = getFileInterface();
    state->dataFile      = setupFile(dbPath);

    state->rules    = NULL;
    state->numRules = 0;

    state->indexFile = NULL;

    if (EMBEDDB_USING_INDEX(parameters)) {
        if (!dbIndexPath) {
            debug_log("ERROR: index file is not provided.");
            return -1;
        }
        state->numIndexPages = calcNumIndexPages(pageNum);
        state->indexFile     = setupFile(dbIndexPath);
    }
    if (EMBEDDB_USING_BMAP(parameters)) {
        setBitmapSize(state, pageNum);
    }

    /* Initialize database */
    if (embedDBInit(state, 1) != 0) {
        embedDBClose(state);
        embedDBtearDown(state);
        return -1;
    }
    debug_log("db: %s", dbPath);
    embedDBPrintInit(state);
    debug_log("state->numIndexPages, %d", state->numIndexPages);
    debug_log("state->bufferSizeInBlocks, %d", state->bufferSizeInBlocks);
    debug_log("state->bitmapSize, %d", state->bitmapSize);
    return 0;
}

int8_t embedDBreset(embedDBState* state, const char* dbPath,
                    const char* dbIndexPath) {
    if (!state)
        return -1;
    uint32_t keySize    = state->keySize;
    uint32_t dataSize   = state->dataSize;
    uint32_t pageSize   = state->pageSize;
    uint32_t pageNum    = state->numDataPages - safetyMargin;
    uint32_t parameters = state->parameters;
    parameters |= EMBEDDB_RESET_DATA;
    embedDBClose(state);
    if (state->fileInterface->removeFile(state->dataFile) != 1)
        return ERR_NOK;

    if (state->indexFile &&
        state->fileInterface->removeFile(state->indexFile) != 1)
        return ERR_NOK;
    embedDBtearDown(state);
    memset(state, 0, sizeof(embedDBState));
    if (embedDBSetup(state, dbPath, dbIndexPath, keySize, dataSize, pageSize,
                     pageNum, parameters) != 0) {
        embedDBClose(state);
        embedDBtearDown(state);
        LOG_ERROR("Error in setuping embedDB.");
        return -1;
    }
    return 0;
}

int8_t embedDBtearDown(embedDBState* state) {
    if (!state)
        return -1;
    state->fileInterface->teardown(state->dataFile);
    state->fileInterface->teardown(state->indexFile);
    EMDB_MEM_FREE(state->buffer);
    EMDB_MEM_FREE(state->fileInterface);
    return 0;
}