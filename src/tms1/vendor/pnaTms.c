#include "pnaTms.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "nth/nth_config.h"
#include "logger.h"
#if TMS_ENABLE_SIGNATURE_PREFLIGHT || TMS_ENABLE_APUPDATE_WRITER
#include "sys/sys.h"
#endif

/* Stage-8 behavior, without the Stage 1-15C diagnostic switchboard. */
enum { PNA_ERR_CONNECT = -100, PNA_ERR_SEND = -101, PNA_ERR_FRAME = -104,
       PNA_ERR_MANIFEST = -105, PNA_ERR_SECTOR = -106,
       PNA_ERR_STORAGE = -107, PNA_ERR_VERIFY = -108,
       PNA_ERR_SERVER = -110, PNA_ERR_PREPARE = -119 };
#define PNA_READ_CHUNK 1024u
#define PNA_READS_PER_TICK 16u
#define PNA_READ_BUDGET_MS 75u
#define PNA_PUMP_BUDGET 6u
#define PNA_READY_CODE 8
#define PNA_PREPARE_CODE 9
#if TMS_ENABLE_SIGNATURE_PREFLIGHT || TMS_ENABLE_APUPDATE_WRITER
#define PNA_ERR_SIGNATURE -122
#endif
#if TMS_ENABLE_APUPDATE_WRITER
#define PNA_DEST_PREPARED_CODE 12
#define PNA_ERR_DEST_BLOCKED -121
#define PNA_APUPDATE_PATH "/mtd2/apupdate"
#define PNA_APUPDATE_SCAN_LIMIT (64u * 1024u * 1024u)
#endif
#if TMS_ENABLE_APUPDATE_AUDIT
#define PNA_AUDIT_CODE 11
#define PNA_ERR_AUDIT -120
/* Legacy macro FILE_TMS_PATH points here. Audit is read-only. */
#if !TMS_ENABLE_APUPDATE_WRITER
#define PNA_APUPDATE_PATH "/mtd2/apupdate"
#define PNA_APUPDATE_SCAN_LIMIT (64u * 1024u * 1024u)
#endif
#endif
#if TMS_ENABLE_COPY_TO_SCRATCH
#define PNA_SCRATCH_CODE 10
/* A test-only file: NEVER point this at APUPDATE. Must not pre-exist. */
#define PNA_SCRATCH_PATH "/mtd0/tms_cp_01.bin"
#endif
#define PNA_LEGACY_HEADER 4096u

bool pnaTmsCopyField(void *dest, size_t destSize,
                     const char *source, size_t sourceCapacity) {
    if (!dest || !source || !destSize) return false;
    size_t length = 0;
    while (length < sourceCapacity && source[length]) ++length;
    if (length > destSize) return false;
    memset(dest, ' ', destSize);
    memcpy(dest, source, length);
    return true;
}

static uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc;
}

static bool progress(PnaTms *p, TmsState state, bool complete) {
    if (!p->active || !p->notify) return false;
    uint8_t percent = 0;
    if (complete) percent = 100;
    else if (p->totalBytes) {
        uint64_t n = p->downloadedBytes * 100u / p->totalBytes;
        percent = (uint8_t)(n > 99u ? 99u : n);
    }
    TmsBackendEvent event = {
        .type = TMS_BACKEND_EVENT_PROGRESS, .state = state,
        .progress = percent,
        .fileIndex = (uint16_t)(p->fileCount ? p->fileIndex + 1 : 0),
        .fileCount = (uint16_t)p->fileCount
    };
    if (event.fileIndex > event.fileCount) event.fileIndex = event.fileCount;
    uint64_t id = p->operationId;
    p->notify(p->notifyContext, id, &event);
    return p->active && p->operationId == id;
}

static void releaseNth(PnaTms *p) {
    if (!p->tx) return;
    p->tx->onConnect = NULL;
    p->tx->onReceive = NULL;
    p->tx->onSent = NULL;
    p->tx->onFailure = NULL;
    p->tx->onTimeout = NULL;
    p->tx->onChunk = NULL;
    p->tx->isComplete = NULL;
    p->tx->receiveLimit = NULL;
    p->tx->userData = NULL;
    nth()->release(p->tx);
    p->tx = NULL;
}

static bool closeFile(PnaTms *p, bool durable) {
    if (!p->opened) return true;
    FileHandle *handle = p->opened;
    p->opened = NULL;
    bool ok = true;
    if (durable) {
        if (OOP_CALL(file(), flush, handle) != 0) ok = false;
        if (OOP_CALL(file(), sync, handle) != 0) ok = false;
    }
    if (OOP_CALL(file(), close, handle) != 0) ok = false;
    return ok;
}

/* Do not delete READY or unknown destinations. The journal owns only the
 * INCOMPLETE paths for which the preflight found no collision. */
static void finish(PnaTms *p, TmsResult result, TmsError error, int code) {
    if (!p || !p->active) return;
    p->active = false;
    p->stage = PNA_TMS_IDLE;
    releaseNth(p);
    bool closed = closeFile(p, false);
#if TMS_ENABLE_COPY_TO_SCRATCH || TMS_ENABLE_APUPDATE_WRITER
    if (p->scratchSource) {
        if (OOP_CALL(file(), close, p->scratchSource) != 0) closed = false;
        p->scratchSource = NULL;
    }
#if TMS_ENABLE_COPY_TO_SCRATCH
    if (p->scratchOwned) {
        /* Never remove a file unless THIS operation successfully created it. */
        if (!closed || OOP_CALL(file(), remove, PNA_SCRATCH_PATH) != 0 ||
            OOP_CALL(file(), exists, PNA_SCRATCH_PATH)) {
            LOG_TRACE("PnaTms: SCRATCH cleanup failed; inspect test file");
            if (result == TMS_RESULT_SUCCESS) {
                result = TMS_RESULT_FAILURE;
                error = TMS_ERR_STORAGE;
                code = PNA_ERR_STORAGE;
            }
        } else LOG_TRACE("PnaTms: SCRATCH removed=1");
        p->scratchOwned = false;
    }
#endif
#if TMS_ENABLE_APUPDATE_WRITER
    if (p->destinationTouched) {
        LOG_TRACE("PnaTms: DEST modified result=%d flag=0 reboot=0", result);
        p->destinationTouched = false;
    }
#endif
#endif
#if TMS_ENABLE_APUPDATE_WRITER
    /* A failed destination close must never be reported as PREPARED. */
    if (!closed && (result == TMS_RESULT_SUCCESS ||
                    result == TMS_RESULT_NO_UPDATE)) {
        result = TMS_RESULT_FAILURE;
        error = TMS_ERR_STORAGE;
        code = PNA_ERR_STORAGE;
    }
#endif
    if (result != TMS_RESULT_SUCCESS && result != TMS_RESULT_NO_UPDATE &&
        p->journalOwned) {
        if (closed && pnaTmsStageDiscard(p->cfg.stagingPrefix, false))
            p->journalOwned = false;
        else LOG_TRACE("PnaTms: incomplete journal retained for recovery");
    }
    TmsBackendNotify cb = p->notify;
    void *cbCtx = p->notifyContext;
    uint64_t id = p->operationId;
    p->notify = NULL;
    p->notifyContext = NULL;
    const TmsBackendEvent event = {
        .type = TMS_BACKEND_EVENT_DONE, .result = result,
        .error = error, .backendCode = code
    };
    LOG_TRACE("PnaTms: done result=%d error=%d code=%d", result, error, code);
    if (cb) cb(cbCtx, id, &event);
}

static void fail(PnaTms *p, TmsError error, int code) {
    finish(p, TMS_RESULT_FAILURE, error, code);
}

static void resetResponse(PnaTms *p) {
    p->rxOffset = 0;
    p->rxLength = 2;
    p->frameReady = false;
    p->frameBad = false;
    p->netFailed = false;
}

static int8_t nthConnected(NthTransaction *tx, void *user) {
    PnaTms *p = (PnaTms *)user;
    (void)tx;
    return p && p->active ? 0 : -1;
}
static int8_t nthReceived(NthTransaction *tx, void *user) {
    PnaTms *p = (PnaTms *)user;
    if (p && p->active && tx == p->tx) p->frameReady = true;
    return 0;
}
static int8_t nthFailed(NthTransaction *tx, void *user) {
    PnaTms *p = (PnaTms *)user;
    if (p && p->active && tx == p->tx) {
        p->netError = tx->lastError;
        p->netFailed = true;
    }
    return 0;
}
static int8_t nthTimeout(NthTransaction *tx, void *user) {
    PnaTms *p = (PnaTms *)user;
    if (p && p->active && tx == p->tx) {
        p->netError = NTH_ERR_TIMEOUT;
        p->netFailed = true;
    }
    return 0;
}
static size_t nthLimit(NthTransaction *tx, void *user) {
    PnaTms *p = (PnaTms *)user;
    if (!p || !p->active || tx != p->tx || p->frameBad ||
        p->rxLength > sizeof(p->rx) || p->rxOffset >= p->rxLength)
        return 0;
    return p->rxLength - p->rxOffset;
}
static void nthChunk(NthTransaction *tx, ByteArray *chunk, void *user) {
    PnaTms *p = (PnaTms *)user;
    if (!p || !p->active || tx != p->tx || !chunk || !chunk->data)
        return;
    if (p->frameBad || p->rxLength > sizeof(p->rx) ||
        p->rxOffset > p->rxLength ||
        chunk->len > p->rxLength - p->rxOffset) {
        p->frameBad = true;
        return;
    }
    memcpy(p->rx + p->rxOffset, chunk->data, chunk->len);
    p->rxOffset += chunk->len;
    if (p->rxLength == 2 && p->rxOffset == 2) {
        size_t expected = 0;
        if (pnaTmsFrameSize(p->rx, &expected) != PNA_PROTO_OK ||
            expected > sizeof(p->rx)) p->frameBad = true;
        else p->rxLength = expected;
    }
}
static NthRxDecision nthComplete(NthTransaction *tx, void *user) {
    PnaTms *p = (PnaTms *)user;
    if (!p || !p->active || tx != p->tx || p->frameBad ||
        p->rxOffset > p->rxLength || p->rxLength > sizeof(p->rx))
        return NTH_RX_ERROR;
    return p->rxLength > 2 && p->rxOffset == p->rxLength ?
        NTH_RX_COMPLETE : NTH_RX_MORE;
}

static bool connectNth(PnaTms *p) {
    releaseNth(p);
    p->tx = nth()->alloc();
    if (!p->tx) { fail(p, TMS_ERR_BUSY, PNA_ERR_CONNECT); return false; }
    resetResponse(p);
    p->tx->keepAlive = true;
    p->tx->streamRx = true;
    p->tx->rxTimeoutInactivity = true;
    p->tx->timeoutMs = p->cfg.ioTimeoutMs ? p->cfg.ioTimeoutMs : NT_DEFAULT_TIMEOUT_MS;
    p->tx->userData = p;
    p->tx->onConnect = nthConnected;
    p->tx->onReceive = nthReceived;
    p->tx->onFailure = nthFailed;
    p->tx->onTimeout = nthTimeout;
    p->tx->onChunk = nthChunk;
    p->tx->isComplete = nthComplete;
    p->tx->receiveLimit = nthLimit;
    NthResult r = nth()->connect(p->tx, p->cfg.host, p->cfg.port);
    if (r != NTH_OK) { fail(p, TMS_ERR_COMMUNICATION, (int)r); return false; }
    p->stage = PNA_TMS_CONNECTING;
    return true;
}

static bool sendPacket(PnaTms *p, const uint8_t *bytes, size_t length) {
    if (!p->tx || p->tx->socketFd < 0 ||
        (p->tx->state != NTH_TX_READY && p->tx->state != NTH_TX_COMPLETED))
        return false;
    resetResponse(p);
    ByteArray ba = { .data = (uint8_t *)bytes, .len = length, .capacity = length };
    if (nth()->send(p->tx, &ba) != NTH_OK) return false;
    p->stage = PNA_TMS_WAIT_RESPONSE;
    return true;
}
static bool sendRequest(PnaTms *p) {
    size_t length = 0;
    PnaProtoResult pr;
    if (!p->manifestReceived) {
        if (!progress(p, TMS_STATE_CHECKING, false)) return false;
        pr = pnaTmsBuildResume(&p->cfg.identity, p->request,
                               sizeof(p->request), &length);
    } else {
        if (p->fileIndex >= p->fileCount ||
            !progress(p, TMS_STATE_DOWNLOADING, false)) return false;
        uint8_t name[PNA_TMS_FILE_NAME_SIZE];
        memset(name, ' ', sizeof(name));
        for (size_t i = 0; i < sizeof(name) && p->files[p->fileIndex].name[i]; ++i)
            name[i] = p->files[p->fileIndex].name[i];
        pr = pnaTmsBuildSectorRequest(name, p->sectorIndex,
                                      p->request, sizeof(p->request), &length);
    }
    if (!p->active) return false;
    if (pr != PNA_PROTO_OK) { fail(p, TMS_ERR_PROTOCOL, PNA_ERR_FRAME); return false; }
    if (!sendPacket(p, p->request, length)) {
        fail(p, TMS_ERR_COMMUNICATION, PNA_ERR_SEND); return false;
    }
    return true;
}

static bool beginFile(PnaTms *p) {
    if (!pnaTmsStageFilePath(p->cfg.stagingPrefix, p->fileIndex,
                              p->currentPath, sizeof(p->currentPath)) ||
        OOP_CALL(file(), exists, p->currentPath)) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
        return false;
    }
    p->opened = OOP_CALL(file(), open, p->currentPath, "wb+");
    if (!p->opened) { fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return false; }
    p->sectorIndex = 0;
    p->sectorAttempt = 0;
    p->downloadedCrc = UINT32_C(0xffffffff);
    p->verifyStarted = false;
    if (p->files[p->fileIndex].size == 0) {
        p->stage = PNA_TMS_VERIFY_FILE;
        return true;
    }
    return sendRequest(p);
}

static void finishReady(PnaTms *p);

static void afterFile(PnaTms *p) {
    ++p->fileIndex;
    if (p->fileIndex < p->fileCount) (void)beginFile(p);
    else if (p->cfg.requireEndPacket) {
        resetResponse(p);
        if (!p->tx || nth()->receiveNext(p->tx) != NTH_OK)
            fail(p, TMS_ERR_COMMUNICATION, PNA_ERR_CONNECT);
        else p->stage = PNA_TMS_WAIT_END;
    } else p->endReceived = true;
}
static void sectorRetry(PnaTms *p, TmsError error, int code) {
    if (!p->manifestReceived || p->fileIndex >= p->fileCount ||
        ++p->sectorAttempt >= p->cfg.maxSectorAttempts) {
        fail(p, error, code);
        return;
    }
    (void)sendRequest(p);
}
static void networkLost(PnaTms *p, TmsError error, int code) {
    if (!p->manifestReceived || p->fileIndex >= p->fileCount ||
        (p->stage != PNA_TMS_CONNECTING && p->stage != PNA_TMS_WAIT_RESPONSE) ||
        ++p->sectorAttempt >= p->cfg.maxSectorAttempts) {
        fail(p, error, code);
        return;
    }
    (void)connectNth(p);
}

static bool preflight(PnaTms *p) {
    PnaTmsStageStatus status = pnaTmsStageInspect(p->cfg.stagingPrefix, NULL);
    if (status == PNA_TMS_STAGE_INCOMPLETE) {
        if (!pnaTmsStageDiscard(p->cfg.stagingPrefix, false)) return false;
    } else if (status != PNA_TMS_STAGE_EMPTY) return false;
    for (size_t i = 0; i < p->fileCount; ++i) {
        char path[PNA_TMS_STAGE_PATH_SIZE];
        if (!pnaTmsStageFilePath(p->cfg.stagingPrefix, i, path, sizeof(path)) ||
            OOP_CALL(file(), exists, path)) return false;
    }
    if (p->cfg.checkStorageSpace &&
        !p->cfg.checkStorageSpace(p->cfg.storageSpaceContext,
                                  p->cfg.stagingPrefix, p->totalBytes))
        return false;
    return true;
}
static void handleManifest(PnaTms *p) {
    size_t count = 0;
    if (pnaTmsParseManifest(p->rx, p->rxLength, p->files,
                             PNA_TMS_MAX_FILES, &count) != PNA_PROTO_OK) {
        fail(p, TMS_ERR_PROTOCOL, PNA_ERR_MANIFEST);
        return;
    }
    if (!count) { finish(p, TMS_RESULT_NO_UPDATE, TMS_ERR_NONE, 0); return; }
    p->manifestReceived = true;
    p->fileCount = count;
    p->fileIndex = 0;
    p->totalBytes = 0;
    for (size_t i = 0; i < count; ++i) p->totalBytes += p->files[i].size;
    if (!preflight(p) ||
        !pnaTmsStageBegin(p->cfg.stagingPrefix, p->files, count,
                          p->totalBytes, &p->journal)) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
        return;
    }
    p->journalOwned = true;
    LOG_TRACE("PnaTms: managed download files=%u bytes=%llu",
              (unsigned)count, (unsigned long long)p->totalBytes);
    (void)beginFile(p);
}
static void handleSector(PnaTms *p) {
    PnaTmsSector sector;
    PnaTmsFile *metadata = &p->files[p->fileIndex];
    uint32_t offset = (uint32_t)p->sectorIndex * PNA_TMS_SECTOR_SIZE;
    if (pnaTmsParseSector(p->rx, p->rxLength, &sector) != PNA_PROTO_OK ||
        offset >= metadata->size) {
        sectorRetry(p, TMS_ERR_PROTOCOL, PNA_ERR_SECTOR); return;
    }
    uint32_t remaining = metadata->size - offset;
    uint16_t expected = remaining > PNA_TMS_SECTOR_SIZE ?
        PNA_TMS_SECTOR_SIZE : (uint16_t)remaining;
    if (sector.index != p->sectorIndex || sector.length != expected) {
        sectorRetry(p, TMS_ERR_PROTOCOL, PNA_ERR_SECTOR); return;
    }
#if LONG_MAX < UINT32_MAX
    if (offset > (uint32_t)LONG_MAX) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
    }
#endif
    if (!p->opened ||
        OOP_CALL(file(), seek, p->opened, (long)offset, FILE_SEEK_ORG_SET) != 0 ||
        OOP_CALL(file(), write, sector.bytes, 1u, sector.length, p->opened) !=
            (size_t)sector.length) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
    }
    p->downloadedCrc = crc32Update(p->downloadedCrc, sector.bytes,
                                   sector.length);
    p->downloadedBytes += sector.length;
    p->sectorAttempt = 0;
    ++p->sectorIndex;
    if (!progress(p, TMS_STATE_DOWNLOADING, false)) return;
    if ((uint32_t)p->sectorIndex * PNA_TMS_SECTOR_SIZE >= metadata->size)
        p->stage = PNA_TMS_VERIFY_FILE;
    else (void)sendRequest(p);
}

static void verifyFile(PnaTms *p) {
    const uint32_t expected = p->files[p->fileIndex].size;
    if (!p->verifyStarted) {
        if (!p->opened || p->downloadedBytes < p->verifiedBytes ||
            p->downloadedBytes - p->verifiedBytes != expected ||
            !closeFile(p, true)) {
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
        }
        p->opened = OOP_CALL(file(), open, p->currentPath, "rb");
        if (!p->opened || OOP_CALL(file(), size, p->opened) != (long)expected) {
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
        }
        p->verifyOffset = 0;
        p->verifyCrc = UINT32_C(0xffffffff);
        pnaTmsMd5Init(&p->verifyMd5);
        p->verifyStarted = true;
        return;
    }
    uint32_t tickStart = NTH_GET_TICK();
    for (unsigned i = 0; i < PNA_READS_PER_TICK &&
                         p->verifyOffset < expected; ++i) {
        size_t length = expected - p->verifyOffset;
        if (length > PNA_READ_CHUNK) length = PNA_READ_CHUNK;
        if (OOP_CALL(file(), read, p->rx, 1u, length, p->opened) != length) {
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
        }
        p->verifyCrc = crc32Update(p->verifyCrc, p->rx, length);
        pnaTmsMd5Update(&p->verifyMd5, p->rx, length);
        p->verifyOffset += (uint32_t)length;
        if ((uint32_t)(NTH_GET_TICK() - tickStart) >= PNA_READ_BUDGET_MS)
            break;
    }
    if (p->verifyOffset < expected) return;
    uint8_t digest[PNA_TMS_MD5_SIZE];
    pnaTmsMd5Final(&p->verifyMd5, digest);
    if (!closeFile(p, false) ||
        p->verifyCrc != p->downloadedCrc ||
        memcmp(digest, p->files[p->fileIndex].md5, sizeof(digest)) != 0) {
        fail(p, TMS_ERR_INTEGRITY, PNA_ERR_VERIFY); return;
    }
    if (!pnaTmsStageMarkVerified(p->cfg.stagingPrefix, &p->journal,
                                  p->fileIndex + 1u)) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
    }
    p->verifiedBytes += expected;
    p->verifyStarted = false;
    LOG_TRACE("PnaTms: verified file=%u/%u bytes=%lu",
              (unsigned)(p->fileIndex + 1u), (unsigned)p->fileCount,
              (unsigned long)expected);
    afterFile(p);
    if (p->active && p->endReceived) finishReady(p);
}
static void finishReady(PnaTms *p) {
    if (p->verifiedBytes != p->totalBytes || !p->endReceived ||
        !pnaTmsStageMarkReady(p->cfg.stagingPrefix, &p->journal)) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
    }
    p->journalOwned = false;
    if (!progress(p, TMS_STATE_VERIFYING, true)) return;
    LOG_TRACE("PnaTms: READY files=%u bytes=%llu install=0 auth=0",
              (unsigned)p->fileCount, (unsigned long long)p->verifiedBytes);
    /* Success means READY staging only; never package activation. */
    finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, PNA_READY_CODE);
}
static void handleFrame(PnaTms *p) {
    PnaTmsCommand command;
    if (pnaTmsValidate(p->rx, p->rxLength, &command) != PNA_PROTO_OK) {
        if (p->manifestReceived && p->fileIndex < p->fileCount &&
            p->stage == PNA_TMS_WAIT_RESPONSE)
            sectorRetry(p, TMS_ERR_PROTOCOL, PNA_ERR_FRAME);
        else fail(p, TMS_ERR_PROTOCOL, PNA_ERR_FRAME);
        return;
    }
    if (command == PNA_CMD_ERROR) {
        PnaTmsServerError error;
        (void)pnaTmsParseError(p->rx, p->rxLength, &error);
        fail(p, TMS_ERR_BACKEND_FAILURE, PNA_ERR_SERVER); return;
    }
    if (p->stage == PNA_TMS_WAIT_END) {
        if (command != PNA_CMD_END ||
            pnaTmsParseEnd(p->rx, p->rxLength) != PNA_PROTO_OK)
            fail(p, TMS_ERR_PROTOCOL, PNA_ERR_FRAME);
        else p->endReceived = true;
    } else if (!p->manifestReceived && command == PNA_CMD_MANIFEST)
        handleManifest(p);
    else if (p->manifestReceived && p->fileIndex < p->fileCount &&
             command == PNA_CMD_SECTOR) handleSector(p);
    else fail(p, TMS_ERR_PROTOCOL, PNA_ERR_FRAME);
}
/* Offline preflight reuses the same backend, buffer and 100 ms pump.
 * READY is not trusted: rehash the entire staged package on every run.
 * This implementation deliberately contains NO destination write path. */
static uint32_t getLe32(const uint8_t *v) {
    return (uint32_t)v[0] | ((uint32_t)v[1] << 8u) |
           ((uint32_t)v[2] << 16u) | ((uint32_t)v[3] << 24u);
}

static bool prepareStart(PnaTms *p) {
    if (pnaTmsStageInspect(p->cfg.stagingPrefix, &p->journal) !=
            PNA_TMS_STAGE_READY_UNTRUSTED || p->journal.fileCount != 1u ||
        p->journal.files[0].size < PNA_LEGACY_HEADER ||
        !pnaTmsStageFilePath(p->cfg.stagingPrefix, 0u,
                              p->currentPath, sizeof(p->currentPath))) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_PREPARE);
        return false;
    }
    p->opened = OOP_CALL(file(), open, p->currentPath, "rb");
    if (!p->opened ||
        OOP_CALL(file(), size, p->opened) != (long)p->journal.files[0].size) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
        return false;
    }
    p->totalBytes = p->journal.files[0].size;
    p->verifyOffset = 0u;
    pnaTmsMd5Init(&p->verifyMd5);
    p->stage = PNA_TMS_PREPARE_MD5;
    LOG_TRACE("PnaTms: PREPARE reverify bytes=%lu readOnly=1",
              (unsigned long)p->journal.files[0].size);
    return true;
}

/* Return false if a legacy embedded name could escape the INI's plain name
 * field or if it is empty; no server-controlled file paths are used. */
static bool parseInnerName(char out[21], const uint8_t *input) {
    size_t n = 0u;
    while (n < 20u && input[n]) {
        uint8_t c = input[n];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
        ++n;
    }
    if (!n || (n == 1u && input[0] == '.') ||
        (n == 2u && input[0] == '.' && input[1] == '.')) return false;
    for (size_t j = n; j < 20u; ++j)
        if (input[j] != 0u && input[j] != ' ') return false;
    memcpy(out, input, n);
    out[n] = 0;
    return true;
}

static bool prepareParseHeader(PnaTms *p) {
    uint32_t headerCrc = crc32Update(UINT32_MAX, p->rx + 4u,
                                     PNA_LEGACY_HEADER - 4u);
    if (getLe32(p->rx) != headerCrc || p->rx[4] == 0u || p->rx[4] > 15u)
        return false;
    p->innerCount = p->rx[4];
    uint64_t offset = PNA_LEGACY_HEADER;
    for (unsigned i = 0u; i < p->innerCount; ++i) {
        /* Packed legacy FileInfo[204]: size +7, crc +11, name +151.
         * Records start at offset 160 in the binary header. */
        const uint8_t *e = p->rx + 160u + 204u * i;
        PnaTmsInnerFile *f = &p->inner[i];
        if (!parseInnerName(f->name, e + 151u)) return false;
        for (unsigned j = 0u; j < i; ++j)
            if (strcmp(f->name, p->inner[j].name) == 0) return false;
        f->size = getLe32(e + 7u);
        f->rawCrc = getLe32(e + 11u);
        if (!f->size || offset > UINT32_MAX || offset > p->totalBytes ||
            f->size > p->totalBytes - offset) return false;
        f->offset = (uint32_t)offset;
        offset += f->size;
    }
    return offset == p->totalBytes;
}

/* Generate the same INI preview as the legacy installer into existing rx.
 * No update destination is opened, created, erased or modified. */
static bool preparePreview(PnaTms *p, size_t *textLen) {
    static const char *section[4] = {
        "[namelist]\r\n", "[adrlist]\r\n",
        "[lenlist]\r\n", "[crclist]\r\n"
    };
    size_t used = 0;
    memset(p->rx, 0, PNA_LEGACY_HEADER);
    for (unsigned s = 0; s < 4u; ++s) {
        size_t n = strlen(section[s]);
        if (n >= PNA_LEGACY_HEADER - used) return false;
        memcpy(p->rx + used, section[s], n);
        used += n;
        for (unsigned i = 0; i < p->innerCount; ++i) {
            const PnaTmsInnerFile *f = &p->inner[i];
            int written;
            if (s == 0u)
                written = snprintf((char *)p->rx + used,
                                   PNA_LEGACY_HEADER - used,
                                   "file%u=%s\r\n", i, f->name);
            else if (s == 1u)
                written = snprintf((char *)p->rx + used,
                                   PNA_LEGACY_HEADER - used,
                                   "adr%u=0x%08X\r\n", i, (unsigned)f->offset);
            else if (s == 2u)
                written = snprintf((char *)p->rx + used,
                                   PNA_LEGACY_HEADER - used,
                                   "len%u=%u\r\n", i, (unsigned)f->size);
            else
                written = snprintf((char *)p->rx + used,
                                   PNA_LEGACY_HEADER - used,
                                   "crc%u=0x%08X\r\n", i,
                                   (unsigned)(f->rawCrc ^ UINT32_MAX));
            if (written < 0 || (size_t)written >= PNA_LEGACY_HEADER - used)
                return false;
            used += (size_t)written;
        }
    }
    *textLen = used;
    return true;
}

/* The vendor's platformVerifyFirmwareFile() verifies each updated region
 * with sdkSysVerifySign(APUPDATE, offset, completedLength). Here every
 * parsed embedded file is selected for the generated update INI, so all
 * these regions must pass the System HAL's trusted signature check.
 * Do not substitute manifest MD5 or CRC32 for a signature. */
#if TMS_ENABLE_SIGNATURE_PREFLIGHT || TMS_ENABLE_APUPDATE_WRITER
static bool verifyEmbeddedSignatures(const PnaTms *p, const char *path) {
#if TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT
    unsigned rejectedCount = 0u;
#endif
    System *system = sys();
    if (!p || !path || !path[0] || !system ||
        !system->vtable.verifySign || p->innerCount == 0u) {
        LOG_TRACE("PnaTms: SIGN unsupported path/system");
        return false;
    }
    for (unsigned i = 0u; i < p->innerCount; ++i) {
        const PnaTmsInnerFile *entry = &p->inner[i];
        if (!entry->size || entry->offset < PNA_LEGACY_HEADER ||
            entry->offset > p->totalBytes ||
            entry->size > p->totalBytes - entry->offset) return false;
        int32_t ret = OOP_CALL(system, verifySign, path,
                               entry->offset, entry->size);
        if (ret != 0) {
            LOG_TRACE("PnaTms: SIGN FAIL file=%u/%u ret=%ld",
                      i + 1u, (unsigned)p->innerCount, (long)ret);
#if TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT
            /* Only an actual SDK rejection can be tolerated, and only
             * in a build with ALL copy/write paths excluded. An absent
             * verifier or invalid metadata still fails closed. */
            ++rejectedCount;
            continue;
#else
            return false;
#endif
        }
        LOG_TRACE("PnaTms: SIGN OK file=%u/%u", i + 1u,
                  (unsigned)p->innerCount);
    }
#if TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT
    if (rejectedCount != 0u)
        LOG_TRACE("PnaTms: SIGN UNVERIFIED debug only rejected=%u/%u",
                  rejectedCount, (unsigned)p->innerCount);
#endif
    return true;
}
#endif

#if TMS_ENABLE_COPY_TO_SCRATCH || TMS_ENABLE_APUPDATE_WRITER
static const char *copyPath(const PnaTms *p) {
#if TMS_ENABLE_APUPDATE_WRITER
    if (p->cfg.prepareDestination) return PNA_APUPDATE_PATH;
#endif
#if TMS_ENABLE_COPY_TO_SCRATCH
    (void)p;
    return PNA_SCRATCH_PATH;
#else
    (void)p;
    return NULL;
#endif
}

#if TMS_ENABLE_APUPDATE_WRITER
/* Never write the protected slot unless BOTH SDK/platform checks are
 * configured and confirm it is idle/ours and the publisher is trusted.
 * The marker is only an additional weak condition, not authorization. */
static bool destinationGuard(PnaTms *p) {
    static const uint8_t marker[7] = {'T','M','S','c','l','e','a'};
    uint8_t head[sizeof(marker)];
    if (!p->cfg.slotIdleAndOwned || !p->cfg.publisherTrusted ||
        !p->cfg.slotIdleAndOwned(p->cfg.safetyContext) ||
        !p->cfg.publisherTrusted(p->cfg.safetyContext, p->currentPath,
                                 p->journal.files[0].md5,
                                 (uint32_t)p->totalBytes)) return false;
    FileHandle *h = OOP_CALL(file(), open, PNA_APUPDATE_PATH, "rb");
    if (!h) return false;
    long size = OOP_CALL(file(), size, h);
    bool ok = size >= (long)p->totalBytes &&
              size <= (long)PNA_APUPDATE_SCAN_LIMIT &&
              OOP_CALL(file(), read, head, 1u, sizeof(head), h) == sizeof(head) &&
              memcmp(head, marker, sizeof(marker)) == 0;
    if (OOP_CALL(file(), close, h) != 0) ok = false;
    return ok;
}
#endif

/* Shared 1024-byte-copy / readback pipeline for disposable scratch or
 * explicitly approved updater destination. Reuses the existing RX buffer. */
static bool scratchStart(PnaTms *p, uint32_t headerCrc) {
    const char *target = copyPath(p);
#if TMS_ENABLE_APUPDATE_WRITER
    if (p->cfg.prepareDestination) {
        if (!destinationGuard(p)) {
            LOG_TRACE("PnaTms: DEST BLOCKED idle/owner/auth/marker");
            fail(p, TMS_ERR_BACKEND_FAILURE, PNA_ERR_DEST_BLOCKED);
            return false;
        }
    } else
#endif
    if (OOP_CALL(file(), exists, target)) {
        LOG_TRACE("PnaTms: SCRATCH refused; path exists");
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
        return false;
    }
    LOG_TRACE("PnaTms: COPY open protected=%d", (int)
#if TMS_ENABLE_APUPDATE_WRITER
              p->cfg.prepareDestination
#else
              0
#endif
              );
    /* T3Rtos open("rb") refuses missing files; open("rb+") creates them.
     * T3Rtos write/overwrite work by pathname, independent of open mode. */
    p->opened = OOP_CALL(file(), open, target,
#if TMS_ENABLE_APUPDATE_WRITER
                          p->cfg.prepareDestination ? "rb" : "wb+"
#else
                          "wb+"
#endif
                          );
    if (!p->opened) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
        return false;
    }
#if TMS_ENABLE_APUPDATE_WRITER
    if (p->cfg.prepareDestination) p->destinationTouched = true;
    else
#endif
    p->scratchOwned = true;
    LOG_TRACE("PnaTms: COPY overwrite header len=4096");
    if (OOP_CALL(file(), overwrite, p->rx, 1u, PNA_LEGACY_HEADER,
                 p->opened) != PNA_LEGACY_HEADER ||
        OOP_CALL(file(), tell, p->opened) != (long)PNA_LEGACY_HEADER) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
        return false;
    }
    p->scratchSource = OOP_CALL(file(), open, p->currentPath, "rb");
    if (!p->scratchSource ||
        OOP_CALL(file(), size, p->scratchSource) != (long)p->totalBytes ||
        OOP_CALL(file(), seek, p->scratchSource, PNA_LEGACY_HEADER,
                 FILE_SEEK_ORG_SET) != 0) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
        return false;
    }
    p->scratchHeaderCrc = headerCrc;
    p->scratchOffset = PNA_LEGACY_HEADER;
    p->downloadedBytes = 0u;
    p->stage = PNA_TMS_SCRATCH_COPY;
    LOG_TRACE("PnaTms: COPY header-first bytes=%lu",
              (unsigned long)p->totalBytes);
    return true;
}

static void scratchProcess(PnaTms *p) {
    uint32_t tick = NTH_GET_TICK();
    if (p->stage == PNA_TMS_SCRATCH_COPY) {
        for (unsigned i = 0u; i < PNA_READS_PER_TICK &&
                              p->scratchOffset < p->totalBytes; ++i) {
            size_t n = p->totalBytes - p->scratchOffset;
            if (n > PNA_READ_CHUNK) n = PNA_READ_CHUNK;
            if (p->scratchOffset == PNA_LEGACY_HEADER)
                LOG_TRACE("PnaTms: COPY first payload write len=%u",
                          (unsigned)n);
            if (OOP_CALL(file(), read, p->rx, 1u, n, p->scratchSource) != n ||
                OOP_CALL(file(), write, p->rx, 1u, n, p->opened) != n) {
                fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
                return;
            }
            p->scratchOffset += (uint32_t)n;
            if ((uint32_t)(NTH_GET_TICK() - tick) >= PNA_READ_BUDGET_MS)
                break;
        }
        if (p->scratchOffset < p->totalBytes) return;
        if (!closeFile(p, true)) {
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
            return;
        }
        if (OOP_CALL(file(), close, p->scratchSource) != 0) {
            p->scratchSource = NULL;
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
            return;
        }
        p->scratchSource = NULL;
        p->opened = OOP_CALL(file(), open, copyPath(p), "rb");
        p->scratchSource = OOP_CALL(file(), open, p->currentPath, "rb");
        if (!p->opened || !p->scratchSource ||
#if TMS_ENABLE_APUPDATE_WRITER
            (p->cfg.prepareDestination ?
             (OOP_CALL(file(), size, p->opened) < (long)p->totalBytes) :
             (OOP_CALL(file(), size, p->opened) != (long)p->totalBytes)) ||
#else
            OOP_CALL(file(), size, p->opened) != (long)p->totalBytes ||
#endif
            OOP_CALL(file(), size, p->scratchSource) != (long)p->totalBytes) {
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
            return;
        }
        /* Bind destination readback to the ORIGINAL verified package, not
         * just to the source file's current contents. The first 4096 bytes
         * of the destination are intentionally replaced with an INI; hash
         * the original binary source header before comparing payload bytes. */
        pnaTmsMd5Init(&p->verifyMd5);
        for (uint32_t off = 0u; off < PNA_LEGACY_HEADER;
             off += PNA_READ_CHUNK) {
            if (OOP_CALL(file(), read, p->rx, 1u, PNA_READ_CHUNK,
                         p->scratchSource) != PNA_READ_CHUNK) {
                fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
                return;
            }
            pnaTmsMd5Update(&p->verifyMd5, p->rx, PNA_READ_CHUNK);
        }
        p->scratchOffset = 0u;
        p->verifyCrc = UINT32_MAX;
        p->stage = PNA_TMS_SCRATCH_VERIFY_HEADER;
        LOG_TRACE("PnaTms: COPY complete; readback");
        return;
    }
    if (p->stage == PNA_TMS_SCRATCH_VERIFY_HEADER) {
        for (unsigned i = 0u; i < PNA_READS_PER_TICK &&
                              p->scratchOffset < PNA_LEGACY_HEADER; ++i) {
            size_t n = PNA_LEGACY_HEADER - p->scratchOffset;
            if (n > PNA_READ_CHUNK) n = PNA_READ_CHUNK;
            if (OOP_CALL(file(), read, p->rx, 1u, n, p->opened) != n) {
                fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
                return;
            }
            p->verifyCrc = crc32Update(p->verifyCrc, p->rx, n);
            p->scratchOffset += (uint32_t)n;
            if ((uint32_t)(NTH_GET_TICK() - tick) >= PNA_READ_BUDGET_MS)
                break;
        }
        if (p->scratchOffset < PNA_LEGACY_HEADER) return;
        if ((p->verifyCrc ^ UINT32_MAX) != p->scratchHeaderCrc) {
            fail(p, TMS_ERR_INTEGRITY, PNA_ERR_VERIFY);
            return;
        }
        p->stage = PNA_TMS_SCRATCH_VERIFY_PAYLOAD;
        return;
    }
    if (p->stage != PNA_TMS_SCRATCH_VERIFY_PAYLOAD) {
        fail(p, TMS_ERR_BACKEND_FAILURE, PNA_ERR_PREPARE);
        return;
    }
    /* src+dst each read in 1024-byte chunks; byte-for-byte comparison. */
    for (unsigned i = 0u; i < PNA_READS_PER_TICK &&
                          p->scratchOffset < p->totalBytes; ++i) {
        size_t n = p->totalBytes - p->scratchOffset;
        if (n > PNA_READ_CHUNK) n = PNA_READ_CHUNK;
        if (OOP_CALL(file(), read, p->rx, 1u, n, p->scratchSource) != n ||
            OOP_CALL(file(), read, p->rx + PNA_READ_CHUNK,
                     1u, n, p->opened) != n ||
            memcmp(p->rx, p->rx + PNA_READ_CHUNK, n) != 0) {
            fail(p, TMS_ERR_INTEGRITY, PNA_ERR_VERIFY);
            return;
        }
        pnaTmsMd5Update(&p->verifyMd5, p->rx, n);
        p->scratchOffset += (uint32_t)n;
        if ((uint32_t)(NTH_GET_TICK() - tick) >= PNA_READ_BUDGET_MS)
            break;
    }
    if (p->scratchOffset < p->totalBytes) return;
    uint8_t verifiedMd5[PNA_TMS_MD5_SIZE];
    pnaTmsMd5Final(&p->verifyMd5, verifiedMd5);
    if (memcmp(verifiedMd5, p->journal.files[0].md5,
               sizeof(verifiedMd5)) != 0) {
        LOG_TRACE("PnaTms: COPY source changed during preparation");
        fail(p, TMS_ERR_INTEGRITY, PNA_ERR_VERIFY);
        return;
    }
#if TMS_ENABLE_APUPDATE_WRITER
    /* The HAL can address files by path; close both read handles before
     * asking the SDK to verify the updater's signed payload regions. */
    if (p->cfg.prepareDestination) {
        bool closed = closeFile(p, false);
        if (p->scratchSource) {
            if (OOP_CALL(file(), close, p->scratchSource) != 0)
                closed = false;
            p->scratchSource = NULL;
        }
        if (!closed) {
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE);
            return;
        }
        if (!verifyEmbeddedSignatures(p, PNA_APUPDATE_PATH)) {
            fail(p, TMS_ERR_INTEGRITY, PNA_ERR_SIGNATURE);
            return;
        }
    }
#endif
    LOG_TRACE("PnaTms: COPY PASS bytes=%lu hdrCRC=%08lX protected=%d",
              (unsigned long)p->totalBytes,
              (unsigned long)p->scratchHeaderCrc,
#if TMS_ENABLE_APUPDATE_WRITER
              (int)p->cfg.prepareDestination
#else
              0
#endif
              );
    if (!progress(p, TMS_STATE_VERIFYING, true)) return;
    /* finish() closes handles. Only the disposable scratch file is removed. */
#if TMS_ENABLE_APUPDATE_WRITER && TMS_ENABLE_COPY_TO_SCRATCH
    int code = p->cfg.prepareDestination ? PNA_DEST_PREPARED_CODE : PNA_SCRATCH_CODE;
#elif TMS_ENABLE_APUPDATE_WRITER
    int code = PNA_DEST_PREPARED_CODE;
#else
    int code = PNA_SCRATCH_CODE;
#endif
    finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, code);
}
#endif /* scratch or protected copy */

#if TMS_ENABLE_APUPDATE_AUDIT
/* Read-only slot classification. The legacy clear-cache routine writes only
 * the first 7 bytes ("TMSclea"); old payload bytes beyond the marker are
 * allowed and counted rather than treated as corruption. Neither a matching
 * marker nor residual data establishes updater ownership or authorization. */
static bool auditApupdateStart(PnaTms *p) {
    p->opened = OOP_CALL(file(), open, PNA_APUPDATE_PATH, "rb");
    if (!p->opened) {
        LOG_TRACE("PnaTms: SLOT not readable; no update write");
        fail(p, TMS_ERR_STORAGE, PNA_ERR_AUDIT);
        return false;
    }
    long physical = OOP_CALL(file(), size, p->opened);
    if (physical < (long)p->totalBytes || physical < 7L ||
        physical > (long)PNA_APUPDATE_SCAN_LIMIT) {
        LOG_TRACE("PnaTms: SLOT size invalid physical=%ld needed=%lu",
                  physical, (unsigned long)p->totalBytes);
        fail(p, TMS_ERR_STORAGE, PNA_ERR_AUDIT);
        return false;
    }
    p->auditSize = (uint32_t)physical;
    p->auditOffset = 0u;
    p->auditResidualBytes = 0u;
    p->stage = PNA_TMS_AUDIT_APUPDATE;
    LOG_TRACE("PnaTms: SLOT scan readOnly=1 size=%lu need=%lu",
              (unsigned long)p->auditSize, (unsigned long)p->totalBytes);
    return true;
}

static void auditApupdateProcess(PnaTms *p) {
    static const uint8_t marker[7] = { 'T','M','S','c','l','e','a' };
    uint32_t started = NTH_GET_TICK();
    for (unsigned i = 0u; i < PNA_READS_PER_TICK &&
                          p->auditOffset < p->auditSize; ++i) {
        uint32_t readAt = p->auditOffset;
        size_t len = p->auditSize - readAt;
        if (len > PNA_READ_CHUNK) len = PNA_READ_CHUNK;
        if (OOP_CALL(file(), read, p->rx, 1u, len, p->opened) != len) {
            LOG_TRACE("PnaTms: SLOT read failure offset=%lu",
                      (unsigned long)readAt);
            fail(p, TMS_ERR_STORAGE, PNA_ERR_AUDIT);
            return;
        }
        for (size_t j = 0u; j < len; ++j) {
            uint32_t at = readAt + (uint32_t)j;
            if (at < sizeof(marker)) {
                /* The marker is an explicit legacy clear-state identifier. */
                if (p->rx[j] != marker[at]) {
                    LOG_TRACE("PnaTms: SLOT marker invalid at=%lu got=%02X",
                              (unsigned long)at, (unsigned)p->rx[j]);
                    fail(p, TMS_ERR_INTEGRITY, PNA_ERR_AUDIT);
                    return;
                }
            } else if (p->rx[j] != 0xffu) {
                /* Clear-cache writes 7 marker bytes; the old payload may remain. */
                if (p->auditResidualBytes == 0u)
                    LOG_TRACE("PnaTms: SLOT residual first=%lu byte=%02X",
                              (unsigned long)at, (unsigned)p->rx[j]);
                ++p->auditResidualBytes;
            }
        }
        p->auditOffset += (uint32_t)len;
        if ((uint32_t)(NTH_GET_TICK() - started) >= PNA_READ_BUDGET_MS)
            break;
    }
    if (p->auditOffset < p->auditSize) return;
    if (!closeFile(p, false)) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_AUDIT);
        return;
    }
    LOG_TRACE("PnaTms: SLOT marker=TMSclea bytes=%lu nonFFTail=%lu",
              (unsigned long)p->auditSize,
              (unsigned long)p->auditResidualBytes);
    LOG_TRACE("PnaTms: SLOT owner=unknown pending=unknown auth=0 write=0 flag=0 reboot=0");
    if (!progress(p, TMS_STATE_VERIFYING, true)) return;
    finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, PNA_AUDIT_CODE);
}
#endif /* TMS_ENABLE_APUPDATE_AUDIT */

static void prepareProcess(PnaTms *p) {
    uint32_t start = NTH_GET_TICK();
    if (p->stage == PNA_TMS_PREPARE_MD5 ||
        p->stage == PNA_TMS_PREPARE_HEADER) {
        const uint32_t target = p->stage == PNA_TMS_PREPARE_MD5 ?
                                p->journal.files[0].size : PNA_LEGACY_HEADER;
        for (unsigned i = 0u; i < PNA_READS_PER_TICK &&
                              p->verifyOffset < target; ++i) {
            size_t len = target - p->verifyOffset;
            if (len > PNA_READ_CHUNK) len = PNA_READ_CHUNK;
            uint8_t *dest = p->stage == PNA_TMS_PREPARE_MD5 ?
                            p->rx : p->rx + p->verifyOffset;
            if (OOP_CALL(file(), read, dest, 1u, len, p->opened) != len) {
                fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
            }
            if (p->stage == PNA_TMS_PREPARE_MD5) {
                pnaTmsMd5Update(&p->verifyMd5, dest, len);
                p->downloadedBytes += len;
            }
            p->verifyOffset += (uint32_t)len;
            if ((uint32_t)(NTH_GET_TICK() - start) >= PNA_READ_BUDGET_MS)
                break;
        }
        if (p->verifyOffset < target) {
            (void)progress(p, TMS_STATE_VERIFYING, false); return;
        }
        if (p->stage == PNA_TMS_PREPARE_MD5) {
            uint8_t digest[PNA_TMS_MD5_SIZE];
            pnaTmsMd5Final(&p->verifyMd5, digest);
            if (memcmp(digest, p->journal.files[0].md5, sizeof(digest)) ||
                OOP_CALL(file(), seek, p->opened, 0, FILE_SEEK_ORG_SET)) {
                fail(p, TMS_ERR_INTEGRITY, PNA_ERR_VERIFY); return;
            }
            p->verifyOffset = 0u;
            p->stage = PNA_TMS_PREPARE_HEADER;
            LOG_TRACE("PnaTms: PREPARE source MD5 match=1");
        } else {
            if (!prepareParseHeader(p)) {
                fail(p, TMS_ERR_INTEGRITY, PNA_ERR_PREPARE); return;
            }
            p->fileIndex = 0u;
            p->verifyOffset = 0u;
            p->verifyCrc = UINT32_MAX;
            if (OOP_CALL(file(), seek, p->opened,
                         (long)p->inner[0].offset, FILE_SEEK_ORG_SET)) {
                fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
            }
            p->stage = PNA_TMS_PREPARE_PAYLOAD;
        }
        return;
    }
    if (p->stage != PNA_TMS_PREPARE_PAYLOAD) {
        fail(p, TMS_ERR_BACKEND_FAILURE, PNA_ERR_PREPARE); return;
    }
    for (unsigned i = 0u; i < PNA_READS_PER_TICK &&
                          p->fileIndex < p->innerCount; ++i) {
        const PnaTmsInnerFile *f = &p->inner[p->fileIndex];
        size_t len = f->size - p->verifyOffset;
        if (len > PNA_READ_CHUNK) len = PNA_READ_CHUNK;
        if (OOP_CALL(file(), read, p->rx, 1u, len, p->opened) != len) {
            fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
        }
        p->verifyCrc = crc32Update(p->verifyCrc, p->rx, len);
        p->verifyOffset += (uint32_t)len;
        if (p->verifyOffset == f->size) {
            if (p->verifyCrc != f->rawCrc) {
                fail(p, TMS_ERR_INTEGRITY, PNA_ERR_PREPARE); return;
            }
            ++p->fileIndex;
            p->verifyOffset = 0u;
            p->verifyCrc = UINT32_MAX;
            if (p->fileIndex < p->innerCount &&
                OOP_CALL(file(), seek, p->opened,
                         (long)p->inner[p->fileIndex].offset,
                         FILE_SEEK_ORG_SET)) {
                fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
            }
        }
        if ((uint32_t)(NTH_GET_TICK() - start) >= PNA_READ_BUDGET_MS)
            break;
    }
    if (p->fileIndex < p->innerCount) return;
    if (!closeFile(p, false)) {
        fail(p, TMS_ERR_STORAGE, PNA_ERR_STORAGE); return;
    }
#if TMS_ENABLE_SIGNATURE_PREFLIGHT || TMS_ENABLE_APUPDATE_WRITER
    /* Authenticate the exact READY package BEFORE any destination write.
     * If this SDK cannot verify /mtd0 paths, fail closed; do not bypass it. */
#if TMS_ENABLE_SIGNATURE_PREFLIGHT
    if (!verifyEmbeddedSignatures(p, p->currentPath)) {
#else
    if (p->cfg.prepareDestination &&
        !verifyEmbeddedSignatures(p, p->currentPath)) {
#endif
        fail(p, TMS_ERR_INTEGRITY, PNA_ERR_SIGNATURE);
        return;
    }
#endif
    size_t textLen = 0;
    if (!preparePreview(p, &textLen)) {
        fail(p, TMS_ERR_INTEGRITY, PNA_ERR_PREPARE); return;
    }
    uint32_t headerCrc = crc32Update(UINT32_MAX, p->rx,
                                     PNA_LEGACY_HEADER) ^ UINT32_MAX;
    LOG_TRACE("PnaTms: PREPARE PASS bytes=%lu inner=%u ini=%lu crc=%08lX",
              (unsigned long)p->journal.files[0].size,
              (unsigned)p->innerCount, (unsigned long)textLen,
              (unsigned long)headerCrc);
#if TMS_ENABLE_APUPDATE_AUDIT
    if (p->cfg.auditApupdate) {
        (void)auditApupdateStart(p);
        return;
    }
#endif
#if TMS_ENABLE_APUPDATE_WRITER
    if (p->cfg.prepareDestination) {
        (void)scratchStart(p, headerCrc);
        return;
    }
#endif
#if TMS_ENABLE_COPY_TO_SCRATCH
    if (p->cfg.copyToScratch) {
        /* The preflight has verified MD5 + all embedded CRCs. */
        (void)scratchStart(p, headerCrc);
        return;
    }
#endif
    LOG_TRACE("PnaTms: PREPARE NO INSTALL write=0 flag=0 reboot=0 auth=0");
    if (!progress(p, TMS_STATE_VERIFYING, true)) return;
    finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, PNA_PREPARE_CODE);
}

static void processPna(void *context) {
    PnaTms *p = (PnaTms *)context;
    if (!p || !p->active) return;
    /* Offline preflight never allocates NTH or writes an update partition. */
    if (p->cfg.prepareOnly) {
#if TMS_ENABLE_APUPDATE_AUDIT
        if (p->stage == PNA_TMS_AUDIT_APUPDATE) {
            auditApupdateProcess(p);
            return;
        }
#endif
#if TMS_ENABLE_COPY_TO_SCRATCH || TMS_ENABLE_APUPDATE_WRITER
        if (p->stage == PNA_TMS_SCRATCH_COPY ||
            p->stage == PNA_TMS_SCRATCH_VERIFY_HEADER ||
            p->stage == PNA_TMS_SCRATCH_VERIFY_PAYLOAD) {
            scratchProcess(p);
            return;
        }
#endif
        prepareProcess(p);
        return;
    }
    /* Application calls nth()->tick() separately, before this pump. */
    for (unsigned transitions = 0; transitions < PNA_PUMP_BUDGET && p->active;
         ++transitions) {
        if (p->netFailed) {
            TmsError err = p->netError == NTH_ERR_TIMEOUT ? TMS_ERR_TIMEOUT :
                (p->netError == NTH_ERR_PROTOCOL ? TMS_ERR_PROTOCOL :
                                                   TMS_ERR_COMMUNICATION);
            p->netFailed = false;
            networkLost(p, err, (int)p->netError);
            return;
        }
        switch (p->stage) {
        case PNA_TMS_CONNECTING:
            if (!p->tx) { fail(p, TMS_ERR_COMMUNICATION, PNA_ERR_CONNECT); return; }
            if (p->tx->state != NTH_TX_READY) return;
            (void)sendRequest(p);
            return;
        case PNA_TMS_WAIT_RESPONSE:
        case PNA_TMS_WAIT_END:
            if (!p->frameReady) return;
            p->frameReady = false;
            handleFrame(p);
            break;
        case PNA_TMS_VERIFY_FILE:
            if (!progress(p, TMS_STATE_VERIFYING, false)) return;
            verifyFile(p);
            return;
        case PNA_TMS_IDLE:
        default:
            fail(p, TMS_ERR_BACKEND_FAILURE, PNA_ERR_FRAME);
            return;
        }
        if (p->active && p->endReceived) {
            finishReady(p);
            return;
        }
    }
}
static bool startPna(void *context, TmsTrigger trigger, uint64_t operationId,
                     TmsBackendNotify notify, void *notifyContext) {
    PnaTms *p = (PnaTms *)context;
    (void)trigger;
    if (!p || !notify || p->active) return false;
    p->active = true;
    p->notify = notify;
    p->notifyContext = notifyContext;
    p->operationId = operationId;
    p->stage = PNA_TMS_IDLE;
    p->fileCount = 0;
    p->fileIndex = 0;
    p->sectorIndex = 0;
    p->sectorAttempt = 0;
    p->manifestReceived = false;
    p->journalOwned = false;
    p->verifyStarted = false;
    p->endReceived = false;
    p->opened = NULL;
    p->tx = NULL;
    p->totalBytes = p->downloadedBytes = p->verifiedBytes = 0;
    p->netFailed = false;
    p->netError = NTH_OK;
    if (p->cfg.prepareOnly) {
        (void)prepareStart(p);
        return true;
    }
    if (!progress(p, TMS_STATE_CONNECTING, false)) return true;
    (void)connectNth(p);
    return true;
}
static bool cancelPna(void *context, uint64_t id) {
    PnaTms *p = (PnaTms *)context;
    if (!p || !p->active || p->operationId != id) return false;
    finish(p, TMS_RESULT_CANCELED, TMS_ERR_NONE, 0);
    return true;
}
bool pnaTmsInit(PnaTms *p, const PnaTmsConfig *cfg) {
    if (!p || !cfg || p->active || cfg->tls ||
#if TMS_ENABLE_APUPDATE_WRITER
        (cfg->prepareDestination && (!cfg->prepareOnly ||
         !cfg->slotIdleAndOwned || !cfg->publisherTrusted)) ||
#endif
        (!cfg->prepareOnly && (!cfg->port || !cfg->host[0] ||
                               !memchr(cfg->host, 0, sizeof(cfg->host)))) ||
        !cfg->stagingPrefix[0] ||
        !memchr(cfg->stagingPrefix, 0, sizeof(cfg->stagingPrefix)) ||
        strlen(cfg->stagingPrefix) > sizeof(cfg->stagingPrefix) - sizeof("_00.bin"))
        return false;
    memset(p, 0, sizeof(*p));
    p->cfg = *cfg;
    if (!p->cfg.maxSectorAttempts) p->cfg.maxSectorAttempts = 3;
    return true;
}
TmsBackend pnaTmsBackend(PnaTms *p) {
    static const TmsBackendOps ops = {
        .start = startPna, .process = processPna, .cancel = cancelPna
    };
    return (TmsBackend){ .ops = &ops, .context = p, .flags = 0 };
}
