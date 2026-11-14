/**
 * @file embedDBsetup.c
 * @brief EmbedDB initialization, first-run creation, reset and resource
 * cleanup.
 *
 * Ownership / lifecycle contract:
 * - Pass a fresh (not currently initialized) embedDBState to embedDBSetup().
 * - On success, call embedDBClose(state) and then embedDBtearDown(state).
 * - embedDBreset() closes and disposes the existing state itself.
 * - getFileInterface() and setupFile() must return owned allocations, as
 * assumed by the original teardown implementation.
 * - The supplied file-interface close callback must tolerate a FILE_INFO whose
 *   internal file handle has not been opened (partial initialization cleanup).
 * - The paths passed to setupFile() must remain valid for its lifetime unless
 *   setupFile() copies the strings itself.
 */

#include "embedDBsetup.h"
#include "logger.h"
#include "common.h"
#include "file/file.h" /* OOP_CALL(file(), exists, path) */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define EMBEDDB_SAFETY_MARGIN_PAGES   2u
#define EMBEDDB_DEFAULT_SPLINE_POINTS 8u
#define EMBEDDB_DEFAULT_INDEX_ERROR   1u

/**
 * @brief Configure the bitmap width and callbacks for an index bitmap.
 *
 * This project supports 8, 16 and 64-bit bitmap implementations. The number
 * of data pages is used as an application-specific width-selection heuristic;
 * it is NOT a computation of the actual data-value domain.
 *
 * @param state Initialized state structure to configure.
 * @param numPages Number of data pages used by the width heuristic.
 */
static void setBitmapSize(embedDBState* state, uint32_t numPages) {
    if (numPages < 8u) {
        state->bitmapSize           = 1u;
        state->inBitmap             = inBitmapInt8;
        state->updateBitmap         = updateBitmapInt8;
        state->buildBitmapFromRange = buildBitmapInt8FromRange;
    } else if (numPages < 16u) {
        state->bitmapSize           = 2u;
        state->inBitmap             = inBitmapInt16;
        state->updateBitmap         = updateBitmapInt16;
        state->buildBitmapFromRange = buildBitmapInt16FromRange;
    } else {
        state->bitmapSize           = 8u;
        state->inBitmap             = inBitmapInt64;
        state->updateBitmap         = updateBitmapInt64;
        state->buildBitmapFromRange = buildBitmapInt64FromRange;
    }
}

/**
 * @brief Calculate the number of RAM page buffers required by EmbedDB.
 *
 * Two buffers are needed for data, plus two for the index (if enabled), plus
 * two for variable data (if enabled).
 *
 * @param parameters EmbedDB feature flags.
 * @return Number of page-sized buffers.
 */
static uint32_t getBufferCount(uint32_t parameters) {
    uint32_t count = 2u;
    if (EMBEDDB_USING_INDEX(parameters))
        count += 2u;
    if (EMBEDDB_USING_VDATA(parameters))
        count += 2u;
    return count;
}

/**
 * @brief Size the index ring in pages (at least four pages).
 *
 * The ~2% heuristic is retained from the original code. Index pages form a
 * circular region, so this is a capacity policy, not a permanent-size proof.
 */
static uint32_t calcNumIndexPages(uint32_t numPages) {
    uint32_t count = numPages / 50u;
    return (count < 4u) ? 4u : count;
}

/**
 * @brief Check that this wrapper can safely initialize the requested layout.
 *
 * Variable data needs an additional file path and variable-page count, neither
 * of which exists in the current public embedDBSetup() signature. Reject it
 * until an explicit configuration API is added.
 *
 * Likewise MAX_MIN needs a correctly typed compareData callback, which this
 * setup function does not accept.
 */
static bool validateConfig(uint16_t keySize, uint16_t dataSize,
                           uint32_t pageSize, uint16_t pageNum,
                           uint32_t parameters) {
    uint32_t bitmapBytes = 0u;
    uint32_t headerBytes;
    uint32_t recordBytes;

    if ((keySize != sizeof(uint32_t) && keySize != sizeof(uint64_t)) ||
        dataSize == 0u || pageNum == 0u || pageSize == 0u ||
        pageSize > UINT16_MAX) {
        LOG_ERROR("EmbedDB: Invalid key/data/page dimensions");
        return false;
    }

    if ((EMBEDDB_USING_INDEX(parameters) ||
         EMBEDDB_USING_RECORD_LEVEL_CONSISTENCY(parameters)) &&
        pageNum < 2u) {
        LOG_ERROR("EmbedDB: At least four total data pages required");
        return false;
    }

    /* initBufferPage() in the supplied EmbedDB.c uses a uint16_t loop
     * counter and always initializes MIN_DATA via a fixed offset (14)
     * even when MAX_MIN is disabled. The checks below protect both. */
    if (pageSize < 14u + 2u * (uint32_t)keySize + (uint32_t)dataSize) {
        LOG_ERROR("EmbedDB: Page too small for initBufferPage layout");
        return false;
    }

    if (EMBEDDB_USING_VDATA(parameters)) {
        LOG_ERROR("EmbedDB: VDATA requires var-file configuration");
        return false;
    }

    if (EMBEDDB_USING_MAX_MIN(parameters)) {
        LOG_ERROR("EmbedDB: MAX_MIN requires compareData configuration");
        return false;
    }

    /* The supplied embedDB.c sizes its data-page bitmap header only if INDEX
     * is enabled, but updates the bitmap whenever BMAP is enabled. It also
     * divides by bitmapSize while initializing the index. Therefore require
     * both flags together with this specific EmbedDB source revision. */
    if (!!EMBEDDB_USING_INDEX(parameters) != !!EMBEDDB_USING_BMAP(parameters)) {
        LOG_ERROR("EmbedDB: INDEX and BMAP must be enabled together");
        return false;
    }

    if (EMBEDDB_USING_INDEX(parameters)) {
        bitmapBytes = (pageNum < 8u) ? 1u : (pageNum < 16u) ? 2u : 8u;
        if (pageSize <= 16u + bitmapBytes) {
            LOG_ERROR("EmbedDB: Page too small for index records");
            return false;
        }
    }

    /* In the supplied EmbedDB implementation, the data header starts with a
     * 6-byte ID/count header. MAX_MIN and VDATA are rejected above. */
    headerBytes = 6u + bitmapBytes;
    recordBytes = (uint32_t)keySize + (uint32_t)dataSize;

    if (pageSize < headerBytes || pageSize - headerBytes < recordBytes) {
        LOG_ERROR("EmbedDB: Page cannot hold one record");
        return false;
    }

    return true;
}

/**
 * @brief Release file objects, buffers and file-interface allocation.
 *
 * Does not call embedDBClose(). An initialized database must be closed first.
 * Accepts partially prepared states and is safe to call more than once.
 */
int8_t embedDBtearDown(embedDBState* state) {
    if (state == NULL)
        return -1;

    if (state->fileInterface != NULL) {
        if (state->fileInterface->teardown != NULL) {
            if (state->dataFile != NULL)
                state->fileInterface->teardown(state->dataFile);
            if (state->indexFile != NULL)
                state->fileInterface->teardown(state->indexFile);
            if (state->varFile != NULL)
                state->fileInterface->teardown(state->varFile);
        }

        /* Assumes getFileInterface() allocated this object dynamically. */
        EMDB_MEM_FREE(state->fileInterface);
    }

    if (state->buffer != NULL)
        EMDB_MEM_FREE(state->buffer);

    memset(state, 0, sizeof(*state));
    return 0;
}

/**
 * @brief Close any EmbedDB resources opened during a failed init attempt.
 *
 * The supplied EmbedDB implementation may return failure after opening only
 * some of its files. The file-interface close callback MUST be NULL-handle
 * tolerant; EmbedDB's own close routine invokes it for each configured file.
 */
static void closeAfterInitAttempt(embedDBState* state) {
    if (state == NULL || state->fileInterface == NULL)
        return;

    if (!EMBEDDB_USING_BINARY_SEARCH(state->parameters) && state->spl == NULL) {
        /* EmbedDB failed before allocating its spline. embedDBClose() would
         * dereference NULL in this revision; close file objects only. */
        if (state->dataFile != NULL)
            state->fileInterface->close(state->dataFile);
        if (state->indexFile != NULL)
            state->fileInterface->close(state->indexFile);
        if (state->varFile != NULL)
            state->fileInterface->close(state->varFile);
        return;
    }

    embedDBClose(state);
}

/** @brief Print initialized EmbedDB configuration and capacity information. */
static logDetails(embedDBState* state) {
    debug_log("====================== Database details ======================");
    debug_log("|##  buffer size in blocks               ##|    = (%lu)",
              state->bufferSizeInBlocks);
    debug_log("|##  page size                           ##|    = (%lu)",
              state->pageSize);
    debug_log("|##  record size                         ##|    = (%lu)",
              state->recordSize);
    debug_log("|##  key size                            ##|    = (%lu)",
              state->keySize);
    debug_log("|##  data size                           ##|    = (%lu)",
              state->dataSize);
    debug_log("|##  number of data pages                ##|    = (%lu)",
              state->numDataPages);
    debug_log("|##  number of index pages               ##|    = (%lu)",
              state->numIndexPages);
    debug_log("|##  max records per page                ##|    = (%lu)",
              state->maxRecordsPerPage);
    debug_log("|##  max records                         ##|    = (%lu)",
              state->maxRecordsPerPage * state->numDataPages);
    debug_log("|##  number of available data Pages      ##|    = (%lu)",
              state->maxRecordsPerPage * state->numAvailDataPages);
    debug_log("|##  use bitmab                          ##|    = (%lu)",
              EMBEDDB_USING_BMAP(state->parameters));
    debug_log("|##  use index                           ##|    = (%lu)",
              EMBEDDB_USING_INDEX(state->parameters));
    debug_log("|##  bitmap size                         ##|    = (%lu)",
              state->bitmapSize);
    debug_log("|##  header size                         ##|    = (%lu)",
              state->headerSize);
    debug_log(
        "===============================================================");
}

/**
 * @brief Configure and initialize an EmbedDB database.
 *
 * Startup policy:
 * - An explicitly requested EMBEDDB_RESET_DATA always starts fresh.
 * - If the primary data file does not exist, create a new database (first run).
 * - If the primary data file exists, reopen it without truncating.
 * - With INDEX enabled, inconsistent data/index file existence is treated as
 *   a storage error, except on an explicit reset. This avoids silently
 *   discarding an existing data file or accepting a stale orphaned index.
 *
 * The RESET_DATA flag is cleared after successful initialization because it
 * is a one-shot request, not a permanent runtime configuration.
 *
 * @param state Fresh, not-yet-initialized state. Do not call while active.
 * @param dbPath Persistent primary data-file path.
 * @param dbIndexPath Persistent index-file path if indexing is enabled.
 * @param keySize Key width (4 or 8 bytes with this wrapper).
 * @param dataSize Fixed-value width in bytes.
 * @param pageSize Storage page size in bytes.
 * @param pageNum Requested number of primary data pages excluding safety
 * margin.
 * @param parameters EmbedDB feature flags (may include RESET_DATA).
 * @return 0 on success, -1 on error. Resource failures leave state zeroed;
 *         preflight validation errors leave the passed state unchanged.
 */
int8_t embedDBSetup(embedDBState* state, const char* dbPath,
                    const char* dbIndexPath, uint16_t keySize,
                    uint16_t dataSize, uint32_t pageSize, uint16_t pageNum,
                    uint16_t parameters) {
    bool     dataExists;
    bool     indexExists = false;
    bool     explicitReset;
    bool     createNew;
    uint32_t effectiveParams;
    size_t   bufferBytes;

    if (state == NULL || dbPath == NULL || dbPath[0] == '\0') {
        LOG_ERROR("EmbedDB: Invalid state or data path");
        return -1;
    }

    explicitReset = ((parameters & EMBEDDB_RESET_DATA) != 0u);

    if (EMBEDDB_USING_INDEX(parameters)) {
        if (dbIndexPath == NULL || dbIndexPath[0] == '\0' ||
            strcmp(dbPath, dbIndexPath) == 0) {
            LOG_ERROR("EmbedDB: Missing/invalid index file path");
            return -1;
        }
    }

    if (!validateConfig(keySize, dataSize, pageSize, pageNum, parameters))
        return -1;

    /* The platform's file() abstraction exposes an existence check. Do not
     * interpret an unsuccessful r+b open as proof of a missing file: it can
     * also indicate I/O failure or corruption. */
    dataExists = OOP_CALL(file(), exists, dbPath);
    if (EMBEDDB_USING_INDEX(parameters))
        indexExists = OOP_CALL(file(), exists, dbIndexPath);

    if (!explicitReset && EMBEDDB_USING_INDEX(parameters) &&
        (dataExists != indexExists)) {
        LOG_ERROR("EmbedDB: File set inconsistent (data=%d index=%d)",
                  (int)dataExists, (int)indexExists);
        return -1;
    }

    createNew       = explicitReset || !dataExists;
    effectiveParams = parameters;
    if (createNew)
        effectiveParams |= EMBEDDB_RESET_DATA;
    else
        effectiveParams &= ~EMBEDDB_RESET_DATA;

    LOG_TRACE("EmbedDB: setup data=%s exists=%d indexExists=%d reset=%d",
              dbPath, (int)dataExists, (int)indexExists, (int)createNew);

    /* This function takes a FRESH state. Resetting a live state here without
     * closing it would leak handles; use embedDBreset() for an active state. */
    memset(state, 0, sizeof(*state));

    state->keySize            = keySize;
    state->dataSize           = dataSize;
    state->pageSize           = pageSize;
    state->recordSize         = (uint32_t)keySize + dataSize;
    state->parameters         = effectiveParams;
    state->eraseSizeInPages   = 1u;
    state->numDataPages       = (uint32_t)pageNum + EMBEDDB_SAFETY_MARGIN_PAGES;
    state->numSplinePoints    = EMBEDDB_DEFAULT_SPLINE_POINTS;
    state->bufferSizeInBlocks = getBufferCount(effectiveParams);
    state->compareKey =
        (keySize == sizeof(uint32_t)) ? int32Comparator : int64Comparator;
    state->rules    = NULL;
    state->numRules = 0;

    if (EMBEDDB_USING_INDEX(effectiveParams)) {
        state->numIndexPages = calcNumIndexPages(pageNum);
        setBitmapSize(state, pageNum);
    }

    if (state->pageSize > SIZE_MAX / state->bufferSizeInBlocks) {
        LOG_ERROR("EmbedDB: Buffer size overflow");
        goto fail;
    }
    bufferBytes   = (size_t)state->bufferSizeInBlocks * state->pageSize;
    state->buffer = EMDB_MEM_ALLOC(bufferBytes);
    if (state->buffer == NULL) {
        LOG_ERROR("EmbedDB: Unable to allocate %lu bytes",
                  (unsigned long)bufferBytes);
        goto fail;
    }
    memset(state->buffer, 0, bufferBytes);

    state->fileInterface = getFileInterface();
    if (state->fileInterface == NULL) {
        LOG_ERROR("EmbedDB: getFileInterface failed");
        goto fail;
    }

    state->dataFile = setupFile(dbPath);
    if (state->dataFile == NULL) {
        LOG_ERROR("EmbedDB: setupFile failed: %s", dbPath);
        goto fail;
    }

    if (EMBEDDB_USING_INDEX(effectiveParams)) {
        state->indexFile = setupFile(dbIndexPath);
        if (state->indexFile == NULL) {
            LOG_ERROR("EmbedDB: setupFile index failed: %s", dbIndexPath);
            goto fail;
        }
    }

    if (embedDBInit(state, EMBEDDB_DEFAULT_INDEX_ERROR) != 0) {
        LOG_ERROR("EmbedDB: embedDBInit failed: %s", dbPath);
        closeAfterInitAttempt(state);
        goto fail;
    }

    /* The RESET_DATA bit affects initialization only. Avoid making it sticky
     * when state->parameters is later reused for configuration/reset calls. */
    state->parameters &= ~EMBEDDB_RESET_DATA;

    LOG_TRACE("EmbedDB: initialized data=%s created=%d", dbPath,
              (int)createNew);
    logDetails(state);
    return 0;

fail:
    embedDBtearDown(state);
    return -1;
}

/**
 * @brief Discard the current database and initialize a fresh one.
 *
 * This is destructive. It relies on the platform's w+b implementation to
 * truncate/create files. Explicit removeFile() calls are intentionally omitted
 * to avoid duplicate deletion, incorrect return-code assumptions and partial
 * cleanup on failure.
 *
 * On failure the state is closed/zeroed and must not be used until a new
 * successful embedDBSetup() call.
 *
 * The path arguments must remain valid across the teardown/reinit sequence.
 *
 * @return 0 on success, -1 on failure.
 */
int8_t embedDBreset(embedDBState* state, const char* dbPath,
                    const char* dbIndexPath) {
    uint16_t keySize;
    uint16_t dataSize;
    uint32_t pageSize;
    uint16_t pageNum;
    uint16_t parameters;
    uint32_t configuredPages;

    if (state == NULL || state->fileInterface == NULL ||
        state->dataFile == NULL || dbPath == NULL || dbPath[0] == '\0') {
        LOG_ERROR("EmbedDB: Cannot reset invalid state");
        return -1;
    }

    configuredPages = state->numDataPages;
    if (configuredPages < EMBEDDB_SAFETY_MARGIN_PAGES ||
        configuredPages - EMBEDDB_SAFETY_MARGIN_PAGES > UINT16_MAX) {
        LOG_ERROR("EmbedDB: Invalid data-page count for reset");
        return -1;
    }

    keySize    = (uint16_t)state->keySize;
    dataSize   = (uint16_t)state->dataSize;
    pageSize   = state->pageSize;
    pageNum    = (uint16_t)(configuredPages - EMBEDDB_SAFETY_MARGIN_PAGES);
    parameters = (uint16_t)(state->parameters | EMBEDDB_RESET_DATA);

    LOG_TRACE("EmbedDB: Reset requested data=%s", dbPath);
    embedDBClose(state);
    embedDBtearDown(state);

    if (embedDBSetup(state, dbPath, dbIndexPath, keySize, dataSize, pageSize,
                     pageNum, parameters) != 0) {
        LOG_ERROR("EmbedDB: Reset failed: %s", dbPath);
        return -1;
    }

    LOG_TRACE("EmbedDB: Reset completed: %s", dbPath);
    return 0;
}
