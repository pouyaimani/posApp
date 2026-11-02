#include "pnaTms.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "nth/nth_config.h"

#define PNA_ATTEMPTS_DEFAULT 3u
#define PNA_PUMP_BUDGET      6u

/* Diagnostic values are intentionally vendor-local. */
enum {
    PNA_DIAG_CONNECT  = -100,
    PNA_DIAG_SEND     = -101,
    PNA_DIAG_RECEIVE  = -102,
    PNA_DIAG_FRAME    = -104,
    PNA_DIAG_MANIFEST = -105,
    PNA_DIAG_SECTOR   = -106,
    PNA_DIAG_STORAGE  = -107,
    PNA_DIAG_VERIFY   = -108,
    PNA_DIAG_INSTALL  = -109,
    PNA_DIAG_SERVER   = -110,
    PNA_DIAG_TLS      = -111,
    PNA_DIAG_NTH_BUSY = -112
};

bool pnaTmsCopyField(void* dst, size_t dstSize, const char* src,
                     size_t srcCapacity) {
    if (!dst || !src || dstSize == 0)
        return false;

    size_t len = 0;

    /* Read no more than the available source capacity. */
    while (len < srcCapacity && src[len] != '\0')
        len++;

    /* Never silently truncate protocol fields. */
    if (len > dstSize)
        return false;

    memset(dst, 0x20, dstSize);
    memcpy(dst, src, len);

    return true;
}

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Log only framing metadata, never the complete server payload or device ID. */
static void logFrameDiagnostic(const PnaTms* p, const char* reason,
                               int protoResult) {
    if (!p)
        return;
    const size_t   have     = p->rxOffset;
    const unsigned b0       = have > 0u ? (unsigned)p->rx[0] : 0u;
    const unsigned b1       = have > 1u ? (unsigned)p->rx[1] : 0u;
    const unsigned b2       = have > 2u ? (unsigned)p->rx[2] : 0u;
    const unsigned b3       = have > 3u ? (unsigned)p->rx[3] : 0u;
    const unsigned b4       = have > 4u ? (unsigned)p->rx[4] : 0u;
    size_t         declared = have >= 2u ? 2u + (((size_t)b0 << 8) | b1) : 0u;
    LOG_TRACE("PnaTms: FRAME DIAG %s proto=%d stage=%d op=%llu "
              "received=%lu expected=%lu declared=%lu "
              "first5=%02X %02X %02X %02X %02X",
              reason, protoResult, (int)p->stage,
              (unsigned long long)p->operationId, (unsigned long)have,
              (unsigned long)p->rxLength, (unsigned long)declared, b0, b1, b2,
              b3, b4);
    if (have >= PNA_TMS_END_SIZE && have == p->rxLength &&
        have <= sizeof(p->rx)) {
        uint8_t calculatedLrc = 0u;
        for (size_t i = 2u; i + 1u < have; ++i)
            calculatedLrc ^= p->rx[i];
        LOG_TRACE("PnaTms: FRAME DIAG tail SO=%02X LRC=%02X calculated=%02X",
                  (unsigned)p->rx[have - 2u], (unsigned)p->rx[have - 1u],
                  (unsigned)calculatedLrc);
    }
}
/* Debug identity quality without exposing serial/terminal identifiers. */
static void logIdentityField(const char* name, const uint8_t* bytes,
                             size_t len) {
    size_t nonblank = 0u;
    size_t nulCount = 0u;
    size_t nonAscii = 0u;
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = bytes[i];
        if (b != (uint8_t)' ' && b != 0u)
            ++nonblank;
        if (b == 0u)
            ++nulCount;
        if (b > 0x7eu || (b < 0x20u && b != 0u))
            ++nonAscii;
    }
    LOG_TRACE("PnaTms: ID field=%s width=%lu nonblank=%lu nul=%lu nonAscii=%lu",
              name, (unsigned long)len, (unsigned long)nonblank,
              (unsigned long)nulCount, (unsigned long)nonAscii);
}

static void logResumeIdentityQuality(const PnaTmsIdentity* id) {
    if (!id)
        return;
    logIdentityField("model", id->model, sizeof(id->model));
    logIdentityField("serial", id->serial, sizeof(id->serial));
    logIdentityField("terminal", id->terminal, sizeof(id->terminal));
    logIdentityField("appVersion", id->appVersion, sizeof(id->appVersion));
    logIdentityField("biosVersion", id->biosVersion, sizeof(id->biosVersion));
    logIdentityField("hwInfo", id->hwInfo, sizeof(id->hwInfo));
}

/* Log raw server error bytes only in diagnostic builds. The server may use a
 * device-specific Persian code page, so sanitizing bytes to '?' hides details.
 * Do not publish these logs before checking them for sensitive data. */
static void logServerErrorBytes(const PnaTmsServerError* error) {
    if (!error)
        return;
    LOG_TRACE("PnaTms: SERVER ERROR dataLenField=%c%c actualMsgBytes=%lu",
              (char)error->lengthField[0], (char)error->lengthField[1],
              (unsigned long)error->messageLength);
    for (size_t off = 0; off < error->messageLength; off += 16u) {
        char   line[16u * 3u + 1u];
        size_t used = 0u;
        size_t end  = error->messageLength - off;
        if (end > 16u)
            end = 16u;
        for (size_t j = 0; j < end; ++j) {
            int n = snprintf(line + used, sizeof(line) - used, "%02X ",
                             (unsigned)error->message[off + j]);
            if (n < 0 || (size_t)n >= sizeof(line) - used)
                break;
            used += (size_t)n;
        }
        line[used] = '\0';
        LOG_TRACE("PnaTms: SERVER ERROR bytes[%02lu..%02lu] %s",
                  (unsigned long)off, (unsigned long)(off + end - 1u), line);
    }
}
#endif

static bool reportProgress(PnaTms* p, TmsState state) {
    if (!p->active || !p->notify)
        return false;
    const uint64_t id = p->operationId;
    uint64_t       fraction =
        p->totalBytes ? (p->downloadedBytes * 100u) / p->totalBytes : 0;
    TmsBackendEvent ev = {
        .type      = TMS_BACKEND_EVENT_PROGRESS,
        .state     = state,
        .progress  = (uint8_t)(fraction > 99u ? 99u : fraction),
        .fileIndex = (uint16_t)(p->fileCount ? p->fileIndex + 1 : 0),
        .fileCount = (uint16_t)p->fileCount};
    if (ev.fileIndex > ev.fileCount)
        ev.fileIndex = ev.fileCount;
    p->notify(p->notifyContext, id, &ev);
    return p->active && p->operationId == id;
}

static void releaseNth(PnaTms* p) {
    if (!p->tx)
        return;
    /* NTH callbacks may no longer reference the backend after release. */
    p->tx->onConnect    = NULL;
    p->tx->onReceive    = NULL;
    p->tx->onSent       = NULL;
    p->tx->onFailure    = NULL;
    p->tx->onTimeout    = NULL;
    p->tx->onChunk      = NULL;
    p->tx->isComplete   = NULL;
    p->tx->receiveLimit = NULL;
    p->tx->userData     = NULL;
    nth()->release(p->tx);
    p->tx         = NULL;
    p->frameReady = false;
    p->frameBad   = false;
    p->netFailed  = false;
}

static bool closeFile(PnaTms* p, bool durable) {
    if (!p->opened)
        return true;
    FileHandle* opened = p->opened;
    p->opened          = NULL;
    bool ok            = true;
    if (durable && OOP_CALL(file(), flush, opened) != 0)
        ok = false;
    if (durable && OOP_CALL(file(), sync, opened) != 0)
        ok = false;
    if (OOP_CALL(file(), close, opened) != 0)
        ok = false;
    return ok;
}

static void discardStaging(PnaTms* p) {
    (void)closeFile(p, false);
    for (size_t i = 0; i < p->stagedCount; ++i) {
        if (p->stagedPaths[i][0])
            (void)OOP_CALL(file(), remove, p->stagedPaths[i]);
    }
    p->stagedCount = 0;
    memset(p->stagedPaths, 0, sizeof(p->stagedPaths));
}

/* All resources are released BEFORE notifying TmsMgr. */
static void finish(PnaTms* p, TmsResult result, TmsError error, int code) {
    if (!p->active)
        return;
    p->active = false;
    p->stage  = PNA_TMS_IDLE;
    releaseNth(p);
    (void)closeFile(p, false);
    if (result != TMS_RESULT_SUCCESS && result != TMS_RESULT_NO_UPDATE)
        discardStaging(p);
    /* On success the installer owns staging lifecycle; do not unlink files. */
    TmsBackendNotify callback      = p->notify;
    void*            notifyContext = p->notifyContext;
    uint64_t         id            = p->operationId;
    p->notify                      = NULL;
    p->notifyContext               = NULL;
    TmsBackendEvent ev             = {.type        = TMS_BACKEND_EVENT_DONE,
                                      .result      = result,
                                      .error       = error,
                                      .backendCode = code};
    LOG_TRACE("PnaTms: complete id=%llu result=%d err=%d code=%d",
              (unsigned long long)id, (int)result, (int)error, code);
    if (callback)
        callback(notifyContext, id, &ev);
}

static void fail(PnaTms* p, TmsError error, int code) {
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p && code == PNA_DIAG_FRAME)
        logFrameDiagnostic(p, "frame-failure", -1);
#endif
    finish(p, TMS_RESULT_FAILURE, error, code);
}

static bool inSectorPhase(const PnaTms* p) {
    return p->manifestReceived && p->fileIndex < p->fileCount &&
           p->stage == PNA_TMS_WAIT_RESPONSE;
}

/* A reconnection attempt is still part of the current sector download. */
static bool inDownloadPhase(const PnaTms* p) {
    return p->manifestReceived && p->fileIndex < p->fileCount &&
           (p->stage == PNA_TMS_WAIT_RESPONSE ||
            p->stage == PNA_TMS_CONNECTING);
}

static void resetResponse(PnaTms* p) {
    p->rxOffset   = 0;
    p->rxLength   = 2;
    p->frameReady = false;
    p->frameBad   = false;
    p->netFailed  = false;
}

/* ---------- NTH callback integration, directly in the vendor backend
 * ---------- */
static int8_t onNthConnect(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    (void)tx;
    return p && p->active ? 0 : -1;
}

static int8_t onNthReceive(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (p && p->active && p->tx == tx)
        p->frameReady = true;
    return 0;
}

static int8_t onNthFailure(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (p && p->active && p->tx == tx) {
        p->netError  = tx->lastError;
        p->netFailed = true;
        LOG_TRACE("PnaTms: NTH failure deferred op=%llu stage=%d nthErr=%d "
                  "nthState=%d",
                  (unsigned long long)p->operationId, (int)p->stage,
                  (int)p->netError, (int)tx->state);
    }
    /* Never release an NTH transaction inside its own callback. */
    return 0;
}

static int8_t onNthTimeout(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (p && p->active && p->tx == tx) {
        p->netError  = NTH_ERR_TIMEOUT;
        p->netFailed = true;
    }
    return 0;
}

static size_t nthReceiveLimit(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (!p || !p->active || p->tx != tx || p->frameBad ||
        p->rxOffset >= p->rxLength)
        return 0;
    return p->rxLength - p->rxOffset;
}

static void onNthChunk(NthTransaction* tx, ByteArray* chunk, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (!p || p->tx != tx || !p->active || !chunk || !chunk->data ||
        p->frameBad || p->rxOffset > p->rxLength ||
        chunk->len > p->rxLength - p->rxOffset) {
        if (p && p->tx == tx)
            p->frameBad = true;
        return;
    }
    memcpy(p->rx + p->rxOffset, chunk->data, chunk->len);
    p->rxOffset += chunk->len;
    if (p->rxLength == 2 && p->rxOffset == 2) {
        size_t         expected    = 0;
        PnaProtoResult frameResult = pnaTmsFrameSize(p->rx, &expected);
        if (frameResult != PNA_PROTO_OK || expected > sizeof(p->rx)) {
            p->frameBad = true;
#if PNA_TMS_ENABLE_DIAGNOSTICS
            logFrameDiagnostic(p, "invalid-length-prefix", (int)frameResult);
#endif
        } else {
            p->rxLength = expected;
#if PNA_TMS_ENABLE_DIAGNOSTICS
            LOG_TRACE("PnaTms: RX header op=%llu prefix=%02X %02X fullLen=%lu",
                      (unsigned long long)p->operationId, (unsigned)p->rx[0],
                      (unsigned)p->rx[1], (unsigned long)expected);
#endif
        }
    }
}

static NthRxDecision nthIsComplete(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (!p || p->tx != tx || !p->active || p->frameBad)
        return NTH_RX_ERROR;
    if (p->rxLength > 2 && p->rxOffset == p->rxLength)
        return NTH_RX_COMPLETE;
    return NTH_RX_MORE;
}

static bool connectNth(PnaTms* p) {
    /* TLS is not exposed by the supplied NTH API. Never silently downgrade. */
    if (p->cfg.tls) {
        fail(p, TMS_ERR_COMMUNICATION, PNA_DIAG_TLS);
        return false;
    }
    releaseNth(p);
    p->tx = nth()->alloc();
    if (!p->tx) {
        fail(p, TMS_ERR_BUSY, PNA_DIAG_NTH_BUSY);
        return false;
    }
    resetResponse(p);
    p->tx->keepAlive           = true;
    p->tx->streamRx            = true;
    p->tx->rxTimeoutInactivity = true;
    p->tx->timeoutMs =
        p->cfg.ioTimeoutMs ? p->cfg.ioTimeoutMs : NT_DEFAULT_TIMEOUT_MS;
    p->tx->userData     = p;
    p->tx->onConnect    = onNthConnect;
    p->tx->onReceive    = onNthReceive;
    p->tx->onFailure    = onNthFailure;
    p->tx->onTimeout    = onNthTimeout;
    p->tx->onChunk      = onNthChunk;
    p->tx->isComplete   = nthIsComplete;
    p->tx->receiveLimit = nthReceiveLimit;
    NthResult result    = nth()->connect(p->tx, p->cfg.host, p->cfg.port);
    if (result != NTH_OK) {
        fail(p, TMS_ERR_COMMUNICATION, (int)result);
        return false;
    }
    p->stage = PNA_TMS_CONNECTING;
    return true;
}

/* Queue a complete request using NTH's own transmission copy. */
static bool sendPacket(PnaTms* p, const uint8_t* bytes, size_t len) {
    if (!p->tx || p->tx->socketFd < 0 ||
        (p->tx->state != NTH_TX_READY && p->tx->state != NTH_TX_COMPLETED))
        return false;
    resetResponse(p);
    ByteArray ba = {.data = (uint8_t*)bytes, .len = len, .capacity = len};
    if (nth()->send(p->tx, &ba) != NTH_OK)
        return false;
    p->stage = PNA_TMS_WAIT_RESPONSE;
    return true;
}

static bool sendRequest(PnaTms* p) {
    size_t         len = 0;
    PnaProtoResult proto;
    if (!p->manifestReceived) {
        if (!reportProgress(p, TMS_STATE_CHECKING))
            return false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
        logResumeIdentityQuality(&p->cfg.identity);
#endif
        proto = pnaTmsBuildResume(&p->cfg.identity, p->request,
                                  sizeof(p->request), &len);
        LOG_TRACE("PnaTms: build command 10 op=%llu bytes=%lu protoResult=%d",
                  (unsigned long long)p->operationId, (unsigned long)len,
                  (int)proto);
    } else {
        if (p->fileIndex >= p->fileCount ||
            !reportProgress(p, TMS_STATE_DOWNLOADING))
            return false;
        proto = pnaTmsBuildSectorRequest(p->files[p->fileIndex].name,
                                         p->sectorIndex, p->request,
                                         sizeof(p->request), &len);
    }
    if (!p->active)
        return false;
    if (proto != PNA_PROTO_OK) {
        fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_FRAME);
        return false;
    }
    if (!sendPacket(p, p->request, len)) {
        fail(p, TMS_ERR_COMMUNICATION, PNA_DIAG_SEND);
        return false;
    }
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANIFEST &&
        !p->manifestReceived) {
        LOG_TRACE("PnaTms: command 10 queued op=%llu bytes=%lu",
                  (unsigned long long)p->operationId, (unsigned long)len);
    }
#endif
    return true;
}

static bool beginFile(PnaTms* p) {
    if (p->fileIndex >= p->fileCount)
        return false;
    const size_t i = p->fileIndex;
    int result     = snprintf(p->stagedPaths[i], sizeof(p->stagedPaths[i]),
                              "%s_%02u.bin", p->cfg.stagingPrefix, (unsigned)i);
    if (result <= 0 || (size_t)result >= sizeof(p->stagedPaths[i])) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return false;
    }
    /* File names from the network are NEVER used as filesystem paths. */
    p->stagedCount = i + 1;
    p->opened      = OOP_CALL(file(), open, p->stagedPaths[i], "wb+");
    if (!p->opened) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return false;
    }
    p->sectorIndex   = 0;
    p->sectorAttempt = 0;
    if (p->files[i].size == 0) {
        p->stage = PNA_TMS_VERIFY_FILE;
        return true;
    }
    return sendRequest(p);
}

static void waitForEndOrInstall(PnaTms* p) {
    if (!p->cfg.requireEndPacket) {
        p->stage = PNA_TMS_INSTALL_START;
        return;
    }
    resetResponse(p);
    if (!p->tx || nth()->receiveNext(p->tx) != NTH_OK) {
        fail(p, TMS_ERR_COMMUNICATION, PNA_DIAG_RECEIVE);
        return;
    }
    p->stage = PNA_TMS_WAIT_END;
}

static void afterFile(PnaTms* p) {
    p->fileIndex++;
    if (p->fileIndex >= p->fileCount)
        waitForEndOrInstall(p);
    else
        (void)beginFile(p);
}

static void sectorRetry(PnaTms* p, TmsError err, int code) {
    if (!inSectorPhase(p) || ++p->sectorAttempt >= p->cfg.maxSectorAttempts) {
        fail(p, err, code);
        return;
    }
    LOG_TRACE("PnaTms: sector retry %u, attempt %u", (unsigned)p->sectorIndex,
              (unsigned)(p->sectorAttempt + 1));
    (void)sendRequest(p);
}

static void networkLost(PnaTms* p, TmsError err, int code) {
    if (!inDownloadPhase(p) || ++p->sectorAttempt >= p->cfg.maxSectorAttempts) {
        fail(p, err, code);
        return;
    }
    LOG_TRACE("PnaTms: reconnect sector %u, attempt %u",
              (unsigned)p->sectorIndex, (unsigned)(p->sectorAttempt + 1));
    releaseNth(p);
    (void)connectNth(p);
}

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Fixed-width wire fields are NOT null-terminated C strings. */
static void logManifestCheckpoint(const PnaTms* p) {
    LOG_TRACE("PnaTms: MANIFEST checkpoint op=%llu files=%lu bytes=%llu",
              (unsigned long long)p->operationId, (unsigned long)p->fileCount,
              (unsigned long long)p->totalBytes);
    for (size_t i = 0; i < p->fileCount; ++i) {
        char        name[PNA_TMS_FILE_NAME_SIZE + 1u];
        char        rawDigestHex[PNA_TMS_MD5_SIZE * 2u + 1u];
        const char* digits = "0123456789ABCDEF";
        size_t      len    = PNA_TMS_FILE_NAME_SIZE;
        for (size_t j = 0; j < PNA_TMS_FILE_NAME_SIZE; ++j) {
            uint8_t ch = p->files[i].name[j];
            name[j]    = (ch >= 0x20u && ch <= 0x7eu) ? (char)ch : '?';
        }
        while (len && name[len - 1u] == ' ')
            --len;
        name[len] = '\0';
        /* Hex display of raw field, not an assumption about its MD5 format. */
        for (size_t j = 0; j < PNA_TMS_MD5_SIZE; ++j) {
            rawDigestHex[j * 2u]      = digits[p->files[i].md5[j] >> 4u];
            rawDigestHex[j * 2u + 1u] = digits[p->files[i].md5[j] & 0x0fu];
        }
        rawDigestHex[PNA_TMS_MD5_SIZE * 2u] = '\0';
        LOG_TRACE("PnaTms: file[%lu] name='%s' size=%lu md5RawHex=%s",
                  (unsigned long)i, name, (unsigned long)p->files[i].size,
                  rawDigestHex);
    }
}
#endif

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Manifest metadata is fixed-width ASCII, not C strings.
 * Print only numeric fields; never log terminal identity, filename or digest.
 */
static void logManifestNumericField(const char* label, const uint8_t* data,
                                    size_t length) {
    static const char hexDigits[] = "0123456789ABCDEF";
    char              ascii[9];
    char              hex[8u * 3u + 1u];
    bool              allDigits = true;

    if (!label || !data || length == 0u || length > 8u)
        return;

    for (size_t i = 0u; i < length; ++i) {
        const uint8_t ch = data[i];
        ascii[i]         = (ch >= 0x20u && ch <= 0x7eu) ? (char)ch : '?';
        if (ch < '0' || ch > '9')
            allDigits = false;
        hex[3u * i]      = hexDigits[ch >> 4u];
        hex[3u * i + 1u] = hexDigits[ch & 0x0fu];
        hex[3u * i + 2u] = ' ';
    }
    ascii[length]    = '\0';
    hex[3u * length] = '\0';
    LOG_TRACE("PnaTms: MANIFEST FIELD %s ascii='%s' hex=%s digits=%d", label,
              ascii, hex, (int)allDigits);
}

static void logManifestFieldFailure(const PnaTms* p) {
    if (!p || p->rxLength < 9u || p->rxLength > sizeof(p->rx))
        return;

    LOG_TRACE(
        "PnaTms: MANIFEST FIELD frameBytes=%lu inferredRecords=%lu maxSize=%lu",
        (unsigned long)p->rxLength, (unsigned long)((p->rxLength - 9u) / 40u),
        (unsigned long)PNA_TMS_MAX_FILE_SIZE);
    logManifestNumericField("count", p->rx + 5u, 2u);

    if (p->rxLength >= 49u) {
        /* First record starts at offset 7. Filename occupies 16 bytes. */
        logManifestNumericField("file[0].size", p->rx + 23u, 8u);
    }
}
#endif

static void handleManifest(PnaTms* p) {
    size_t         count  = 0;
    PnaProtoResult parsed = pnaTmsParseManifest(p->rx, p->rxLength, p->files,
                                                PNA_TMS_MAX_FILES, &count);
    if (parsed != PNA_PROTO_OK) {
        LOG_TRACE("PnaTms: invalid manifest op=%llu bytes=%lu protoError=%d",
                  (unsigned long long)p->operationId,
                  (unsigned long)p->rxLength, (int)parsed);
#if PNA_TMS_ENABLE_DIAGNOSTICS
        logManifestFieldFailure(p);
#endif
        fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_MANIFEST);
        return;
    }
    p->manifestReceived = true;
    p->fileCount        = count;
    p->fileIndex        = 0;
    p->totalBytes       = 0;
    for (size_t i = 0; i < count; ++i)
        p->totalBytes += p->files[i].size;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANIFEST) {
        /* Stop before beginFile(): zero storage and installation side effects.
         */
        logManifestCheckpoint(p);
        finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
               (int)PNA_TMS_CHECKPOINT_MANIFEST);
        return;
    }
#endif
    if (count == 0) {
        finish(p, TMS_RESULT_NO_UPDATE, TMS_ERR_NONE, 0);
        return;
    }
    (void)beginFile(p);
}

static void handleSector(PnaTms* p) {
    PnaTmsSector      sector;
    const PnaTmsFile* metadata = &p->files[p->fileIndex];
    uint32_t          offset   = (uint32_t)p->sectorIndex * PNA_TMS_SECTOR_SIZE;
    if (pnaTmsParseSector(p->rx, p->rxLength, &sector) != PNA_PROTO_OK ||
        offset >= metadata->size) {
        sectorRetry(p, TMS_ERR_PROTOCOL, PNA_DIAG_SECTOR);
        return;
    }
    uint32_t remaining = metadata->size - offset;
    uint16_t expected  = remaining > PNA_TMS_SECTOR_SIZE ? PNA_TMS_SECTOR_SIZE
                                                         : (uint16_t)remaining;
    if (sector.index != p->sectorIndex || sector.length != expected) {
        sectorRetry(p, TMS_ERR_PROTOCOL, PNA_DIAG_SECTOR);
        return;
    }
#if LONG_MAX < UINT32_MAX
    if (offset > (uint32_t)LONG_MAX) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
#endif
    if (!p->opened ||
        OOP_CALL(file(), seek, p->opened, (long)offset, FILE_SEEK_ORG_SET) !=
            0 ||
        OOP_CALL(file(), write, sector.bytes, 1u, sector.length, p->opened) !=
            sector.length) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    p->downloadedBytes += sector.length;
    p->sectorAttempt = 0;
    ++p->sectorIndex;
    if (!reportProgress(p, TMS_STATE_DOWNLOADING))
        return;
    if ((uint32_t)p->sectorIndex * PNA_TMS_SECTOR_SIZE >= metadata->size)
        p->stage = PNA_TMS_VERIFY_FILE;
    else
        (void)sendRequest(p);
}

static void handleFrame(PnaTms* p) {
    PnaTmsCommand  command;
    PnaProtoResult validation = pnaTmsValidate(p->rx, p->rxLength, &command);
    if (validation != PNA_PROTO_OK) {
#if PNA_TMS_ENABLE_DIAGNOSTICS
        logFrameDiagnostic(p, "validation-failed", (int)validation);
#endif
        if (inSectorPhase(p))
            sectorRetry(p, TMS_ERR_PROTOCOL, PNA_DIAG_FRAME);
        else
            fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_FRAME);
        return;
    }
    LOG_TRACE("PnaTms: received command=%d frameBytes=%lu op=%llu",
              (int)command, (unsigned long)p->rxLength,
              (unsigned long long)p->operationId);
    if (command == PNA_CMD_ERROR) {
        PnaTmsServerError serverError;
        if (pnaTmsParseError(p->rx, p->rxLength, &serverError) !=
            PNA_PROTO_OK) {
            fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_SERVER);
        } else {
            char message[sizeof(serverError.message) + 1u];
            for (size_t i = 0; i < serverError.messageLength; ++i) {
                uint8_t ch = serverError.message[i];
                message[i] = (ch >= 0x20u && ch <= 0x7eu) ? (char)ch : '?';
            }
            message[serverError.messageLength] = '\0';
            LOG_TRACE("PnaTms: server error msg='%s' bytes=%lu", message,
                      (unsigned long)serverError.messageLength);
#if PNA_TMS_ENABLE_DIAGNOSTICS
            logServerErrorBytes(&serverError);
#endif
            fail(p, TMS_ERR_BACKEND_FAILURE, PNA_DIAG_SERVER);
        }
        return;
    }
    if (p->stage == PNA_TMS_WAIT_END) {
        if (command != PNA_CMD_END ||
            pnaTmsParseEnd(p->rx, p->rxLength) != PNA_PROTO_OK)
            fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_FRAME);
        else
            p->stage = PNA_TMS_INSTALL_START;
        return;
    }
    if (!p->manifestReceived && command == PNA_CMD_MANIFEST) {
        handleManifest(p);
        return;
    }
    if (inSectorPhase(p) && command == PNA_CMD_SECTOR) {
        handleSector(p);
        return;
    }
#if PNA_TMS_ENABLE_DIAGNOSTICS
    LOG_TRACE("PnaTms: unexpected command=%d stage=%d manifestReceived=%d",
              (int)command, (int)p->stage, (int)p->manifestReceived);
    logFrameDiagnostic(p, "unexpected-command", (int)PNA_PROTO_OK);
#endif
    fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_FRAME);
}

static void processPna(void* context) {
    PnaTms* p = (PnaTms*)context;
    if (!p || !p->active)
        return;
    /* NTH is ticked by the application, not from this function. */
    for (unsigned transitions = 0; transitions < PNA_PUMP_BUDGET && p->active;
         ++transitions) {
        if (p->netFailed || (p->tx && p->tx->state == NTH_TX_FAILED)) {
            TmsError err =
                p->netError == NTH_ERR_TIMEOUT
                    ? TMS_ERR_TIMEOUT
                    : (p->netError == NTH_ERR_PROTOCOL ? TMS_ERR_PROTOCOL
                                                       : TMS_ERR_COMMUNICATION);
            LOG_TRACE(
                "PnaTms: processing NTH failure op=%llu stage=%d error=%d",
                (unsigned long long)p->operationId, (int)p->stage,
                (int)p->netError);
            networkLost(p, err, (int)p->netError);
            return;
        }
        switch (p->stage) {
        case PNA_TMS_CONNECTING:
            if (!p->tx) {
                fail(p, TMS_ERR_COMMUNICATION, PNA_DIAG_CONNECT);
                return;
            }
            if (p->tx->state != NTH_TX_READY)
                return;
#if PNA_TMS_ENABLE_DIAGNOSTICS
            if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_CONNECTED) {
                /* Real NTH connection, no packet transmitted or file opened.
                 * Report a diagnostic result, not UPDATE SUCCESS.
                 * finish() releases the NTH socket BEFORE notifying TmsMgr. */
                LOG_TRACE("PnaTms: CONNECTED checkpoint op=%llu fd=%d",
                          (unsigned long long)p->operationId, p->tx->socketFd);
                finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
                       (int)PNA_TMS_CHECKPOINT_CONNECTED);
                return;
            }
#endif
            (void)sendRequest(p);
            return;
        case PNA_TMS_WAIT_RESPONSE:
        case PNA_TMS_WAIT_END:
            if (!p->frameReady)
                return;
            p->frameReady = false;
            handleFrame(p);
            break;
        case PNA_TMS_VERIFY_FILE: {
            if (!reportProgress(p, TMS_STATE_VERIFYING))
                return;
            const size_t index = p->fileIndex;
            if (!p->opened || index >= p->fileCount) {
                fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                return;
            }
            long sz        = OOP_CALL(file(), size, p->opened);
            bool validSize = sz >= 0 && (uint64_t)sz == p->files[index].size;
            if (!closeFile(p, true) || !validSize) {
                fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                return;
            }
            if (!p->cfg.package.verify(p->cfg.package.context, &p->files[index],
                                       p->stagedPaths[index])) {
                fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
                return;
            }
            afterFile(p);
            break;
        }
        case PNA_TMS_INSTALL_START: {
            if (!reportProgress(p, TMS_STATE_INSTALLING))
                return;
            p->installationStarted = true;
            int ret                = p->cfg.package.beginInstall(
                p->cfg.package.context, p->files,
                (const char (*)[PNA_TMS_STAGE_PATH_SIZE])p->stagedPaths,
                p->fileCount);
            if (ret < 0)
                fail(p, TMS_ERR_INSTALLATION, PNA_DIAG_INSTALL);
            else if (ret > 0)
                finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, 0);
            else
                p->stage = PNA_TMS_INSTALL_WAIT;
            return;
        }
        case PNA_TMS_INSTALL_WAIT: {
            int ret = p->cfg.package.pollInstall(p->cfg.package.context);
            if (ret < 0)
                fail(p, TMS_ERR_INSTALLATION, PNA_DIAG_INSTALL);
            else if (ret > 0)
                finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, 0);
            return;
        }
        case PNA_TMS_IDLE:
        default:
            fail(p, TMS_ERR_BACKEND_FAILURE, PNA_DIAG_FRAME);
            return;
        }
    }
}

static bool startPna(void* context, TmsTrigger trigger, uint64_t operationId,
                     TmsBackendNotify notify, void* notifyContext) {
    PnaTms* p = (PnaTms*)context;
    if (!p || !notify || p->active)
        return false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    /* A test checkpoint must never replace a real scheduled/automatic update.
     */
    if (p->cfg.checkpoint != PNA_TMS_CHECKPOINT_NONE &&
        trigger != TMS_TRIGGER_MANUAL)
        return false;
#else
    (void)trigger;
#endif
    /* Clean old completed staging before a new operation, not during install.
     */
    if (p->stagedCount)
        discardStaging(p);
    p->active              = true;
    p->notify              = notify;
    p->notifyContext       = notifyContext;
    p->operationId         = operationId;
    p->fileCount           = 0;
    p->fileIndex           = 0;
    p->sectorIndex         = 0;
    p->sectorAttempt       = 0;
    p->manifestReceived    = false;
    p->totalBytes          = 0;
    p->downloadedBytes     = 0;
    p->installationStarted = false;
    p->netError            = NTH_OK;
    if (!reportProgress(p, TMS_STATE_CONNECTING))
        return true;
    (void)connectNth(p); /* Error completion can be synchronous. */
    return true;
}

static bool cancelPna(void* context, uint64_t id) {
    PnaTms* p = (PnaTms*)context;
    if (!p || !p->active || p->operationId != id || p->installationStarted ||
        p->stage == PNA_TMS_INSTALL_START)
        return false;
    finish(p, TMS_RESULT_CANCELED, TMS_ERR_NONE, 0);
    return true;
}

bool pnaTmsInit(PnaTms* p, const PnaTmsConfig* cfg) {
    if (!p || !cfg || p->active ||
        !memchr(cfg->host, '\0', sizeof(cfg->host)) || !cfg->host[0] ||
        cfg->port == 0)
        return false;

    /* Checkpoint testing cannot write or install. Full update requires hooks.
     */
    bool diagnostic = false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (cfg->checkpoint != PNA_TMS_CHECKPOINT_NONE &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_CONNECTED &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_MANIFEST)
        return false;
    diagnostic = cfg->checkpoint != PNA_TMS_CHECKPOINT_NONE;
#endif
    if (!diagnostic) {
        if (cfg->stagingPrefix[0] == '\0' ||
            !memchr(cfg->stagingPrefix, '\0', sizeof(cfg->stagingPrefix)) ||
            !cfg->package.verify || !cfg->package.beginInstall ||
            !cfg->package.pollInstall)
            return false;
        if (strlen(cfg->stagingPrefix) + sizeof("_00.bin") >
            PNA_TMS_STAGE_PATH_SIZE)
            return false;
    }
    memset(p, 0, sizeof(*p));
    p->cfg = *cfg;
    if (!p->cfg.maxSectorAttempts)
        p->cfg.maxSectorAttempts = PNA_ATTEMPTS_DEFAULT;
    return true;
}

TmsBackend pnaTmsBackend(PnaTms* p) {
    static const TmsBackendOps ops = {
        .start = startPna, .process = processPna, .cancel = cancelPna};
    return (TmsBackend){.ops = &ops, .context = p, .flags = 0};
}
