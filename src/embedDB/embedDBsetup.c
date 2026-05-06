#include "embedDBsetup.h"
#include "logger.h"
#include <math.h>

static inline void setBitmapSize(embedDBState *state, uint32_t numPages) {
    // Roughly one bucket per ~4 pages, but clamped
    uint32_t buckets = numPages / 4;

    if (buckets < 8) {
        buckets = 8;    // minimum useful
    } else if (buckets < 16) {
        buckets = 16;
    } else {
        buckets = 64;   // maximum realistic
    }
    state->bitmapSize = (buckets + 7) / 8;   // round up

    if (state->bitmapSize == 1) {
        state->inBitmap = inBitmapInt8;
        state->updateBitmap = updateBitmapInt8;
        state->buildBitmapFromRange = buildBitmapInt8FromRange;
    } else if(state->bitmapSize == 2) {
        state->inBitmap = inBitmapInt16;
        state->updateBitmap = updateBitmapInt16;
        state->buildBitmapFromRange = buildBitmapInt16FromRange;
    } else if (state->bitmapSize == 8) {
        state->inBitmap = inBitmapInt64;
        state->updateBitmap = updateBitmapInt64;
        state->buildBitmapFromRange = buildBitmapInt64FromRange;
    }
}

static int getBufferCount(uint32_t parameters) {
    int count = 2;
    if(EMBEDDB_USING_INDEX(parameters)) count += 2;
    if(EMBEDDB_USING_VDATA(parameters)) count += 2;
    return count;

}
static inline uint32_t calcNumIndexPages(uint32_t numPages)
{
    uint32_t n = numPages / 50;    // ≈2%
    if (n < 4) n = 4;              // minimum required for stability
    return n;
}


int8_t embedDBSetup(embedDBState *state,
                    const char *dbPath,
                    const char *dbIndexPath,
                    uint16_t keySize,
                    uint16_t dataSize,
                    uint32_t pageSize,
                    uint16_t pageNum)
{
    if (!state) 
        return -1;
    /* Basic configuration */
    state->keySize = keySize;
    state->dataSize = dataSize;
    state->parameters |= EMBEDDB_RECORD_LEVEL_CONSISTENCY;

    state->recordSize = keySize + dataSize;
    /* Compute page size */
    state->pageSize = pageSize;

    /* Default erase block size */
    state->eraseSizeInPages = 1;
    uint16_t safetyMargin = 2;
    state->numDataPages = pageNum + safetyMargin;

        /* Minimum buffers */
    state->bufferSizeInBlocks = getBufferCount(state->parameters);
    size_t bufferSize = state->bufferSizeInBlocks * state->pageSize;
    state->buffer = EMDB_MEM_ALLOC(bufferSize);
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
    state->compareKey = int32Comparator;
    // state->compareData = dataComparator;

    state->fileInterface = getFileInterface();
    state->dataFile = setupFile(dbPath);

    state->rules = NULL;
    state->numRules = 0;

    if (dbIndexPath) {
        state->parameters |= (EMBEDDB_USE_BMAP | EMBEDDB_USE_INDEX);
        setBitmapSize(state, pageNum);
        state->numIndexPages = calcNumIndexPages(pageNum);
        state->indexFile = setupFile(dbIndexPath);
    } else {
        state->indexFile = NULL;
    }


    /* Initialize database */
    int ret = embedDBInit(state, 1);
    if (ret != 0) {
        embedDBtearDown(state);
        return -1;
    }

    static bool once = true;
    if (once) {
        embedDBPrintInit(state);
        debug_log("state->numIndexPages, %d", state->numIndexPages);
        debug_log("state->bufferSizeInBlocks, %d", state->bufferSizeInBlocks);
        debug_log("state->bitmapSize, %d", state->bitmapSize);
        once = false;
    }

    return ret;
}

int8_t embedDBreset(embedDBState *state,
                    const char *dbPath,
                    const char *dbIndexPath,
                    uint16_t keySize,
                    uint16_t dataSize,
                    uint32_t pageSize,
                    uint16_t pageNum) {
    if (!state) 
        return -1;                        
    state->fileInterface->removeFile(dbPath);
    state->fileInterface->removeFile(dbIndexPath);
    memset(state, 0 , sizeof(embedDBState));
    state->parameters |= EMBEDDB_RESET_DATA;
    bool reseted = false;
    if (embedDBSetup(state, dbPath, dbIndexPath, keySize,
             dataSize, pageSize, pageNum) == 0) {
                reseted = true;
    }
    embedDBClose(state);
    embedDBtearDown(state);
    if (!reseted) {
        debug_log("embedDB: Error in reseting embedDB.");
    }
    return reseted ? 0 : -1;
}

int8_t embedDBtearDown(embedDBState *state) {
    if (!state) 
        return -1;
    state->fileInterface->teardown(state->dataFile);
    state->fileInterface->teardown(state->indexFile);
    EMDB_MEM_FREE(state->buffer);
    EMDB_MEM_FREE(state->fileInterface);
    return 0;
}