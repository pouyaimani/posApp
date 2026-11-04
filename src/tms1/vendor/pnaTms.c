#include "pnaTms.h"

#include <limits.h>
#include <string.h>
#include "nth/nth_config.h"
#include "logger.h"

/* Stage-8 behavior, without the Stage 1-15C diagnostic switchboard. */
enum { PNA_ERR_CONNECT = -100, PNA_ERR_SEND = -101, PNA_ERR_FRAME = -104,
       PNA_ERR_MANIFEST = -105, PNA_ERR_SECTOR = -106,
       PNA_ERR_STORAGE = -107, PNA_ERR_VERIFY = -108,
       PNA_ERR_SERVER = -110 };
#define PNA_READ_CHUNK 1024u
#define PNA_READS_PER_TICK 16u
#define PNA_READ_BUDGET_MS 75u
#define PNA_PUMP_BUDGET 6u
#define PNA_READY_CODE 8

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
static void processPna(void *context) {
    PnaTms *p = (PnaTms *)context;
    if (!p || !p->active) return;
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
    if (!p || !cfg || p->active || cfg->tls || !cfg->port || !cfg->host[0] ||
        !memchr(cfg->host, 0, sizeof(cfg->host)) ||
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
