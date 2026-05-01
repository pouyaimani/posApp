#include "embedDBsetup.h"
#include "logger.h"
#include <math.h>

#define HEADER_SIZE 6

#define POW2(n) (1 << (n))

#define TWO_MAX_POWER 32

static uint32_t pick_page_size(uint32_t size)
{
    for (size_t i = 0; i < TWO_MAX_POWER; i++) {
        uint32_t candidate = pow(2,i);
        if (candidate > size) {
            return candidate;
        }
    }
    return 0;
}


int8_t embedDBSetup(embedDBState *state,
                    const char *dbName,
                    uint16_t keySize,
                    uint16_t dataSize,
                    uint32_t pageNum,
                    uint16_t recordsPerPage)
{
    uint32_t recordSize = keySize + dataSize;

    /* Basic configuration */
    state->keySize = keySize;
    state->dataSize = dataSize;
    state->parameters = EMBEDDB_RECORD_LEVEL_CONSISTENCY;

    state->recordSize = recordSize;

    /* Default header size */
    state->headerSize = 6;

    /* Compute page size */
    state->pageSize = pick_page_size(state->headerSize + recordSize * recordsPerPage);

    /* Default erase block size */
    state->eraseSizeInPages = 1;

    /* Ensure page count aligns with erase blocks */
    uint32_t alignedPages =
        (pageNum / state->eraseSizeInPages) * state->eraseSizeInPages;

    if (alignedPages < 2 * state->eraseSizeInPages) {
#ifdef PRINT_ERRORS
        debug_log("ERROR: Not enough pages for embedDB.\n");
#endif
        return -1;
    }

    state->numDataPages = alignedPages;

    /* Minimum buffers */
    state->bufferSizeInBlocks = 2;
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
    state->dataFile = setupFile(dbName);

    state->rules = NULL;
    state->numRules = 0;

    if (recordsPerPage < 2) {
#ifdef PRINT_ERRORS
        debug_log("ERROR: records per page must be greater than 1.");
#endif
        return -1;        
    }

    /* Initialize database */
    return embedDBInit(state, recordsPerPage / 2);
}

int8_t embedDBtearDown(embedDBState *state) {
    state->fileInterface->teardown(state->dataFile);
    EMDB_MEM_FREE(state->buffer);
    EMDB_MEM_FREE(state->fileInterface);
}