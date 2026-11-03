#include "pnaTms.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "nth/nth.h"
#include "nth/nth_config.h"
#include "file/file.h"

#define PNA_ATTEMPTS_DEFAULT 3u
#define PNA_PUMP_BUDGET      6u
#define PNA_STAGE4_READ_CHUNK 128u
#define PNA_STAGE5_READ_CHUNK 256u
/* Stage 6/7: keep each File HAL read below the device's 1028-byte limit,
 * while performing several bounded reads per process() call. No additional
 * buffer is allocated: rx is reused after the last sector has been received.
 * Maximum per call is 16 x 1024 = 16384 bytes (subject to the time budget).
 */
#define PNA_STAGE6_READ_CHUNK      1024u
#define PNA_STAGE6_READS_PER_TICK  16u
#define PNA_STAGE6_TICK_BUDGET_MS  75u

#if PNA_STAGE6_READ_CHUNK > 1028u
#error "Stage 6 File HAL read exceeds device limit"
#endif

/* The following function is also used to prepare the fixed-width identity.
 * It was already copied into the user's pnaTms.c; keep exactly one definition. */
bool pnaTmsCopyField(void* dst, size_t dstSize, const char* src,
                     size_t srcCapacity) {
    if (!dst || !src || dstSize == 0u) return false;
    size_t len = 0u;
    while (len < srcCapacity && src[len] != '\0') ++len;
    if (len > dstSize) return false;
    memset(dst, 0x20, dstSize);
    memcpy(dst, src, len);
    return true;
}

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* CRC32 here checks local file readback, NOT package authenticity.
 * Trusted verification of app/firmware packages remains a separate step. */
static uint32_t stage5Crc32(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0u; i < len; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1u) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc;
}
#endif

/* Diagnostic values are intentionally vendor-local. */
enum {
    PNA_DIAG_CONNECT = -100,
    PNA_DIAG_SEND = -101,
    PNA_DIAG_RECEIVE = -102,
    PNA_DIAG_FRAME = -104,
    PNA_DIAG_MANIFEST = -105,
    PNA_DIAG_SECTOR = -106,
    PNA_DIAG_STORAGE = -107,
    PNA_DIAG_VERIFY = -108,
    PNA_DIAG_INSTALL = -109,
    PNA_DIAG_SERVER = -110,
    PNA_DIAG_TLS = -111,
    PNA_DIAG_NTH_BUSY = -112
};

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Log only framing metadata, never the complete server payload or device ID. */
static void logFrameDiagnostic(const PnaTms* p, const char* reason, int protoResult) {
    if (!p) return;
    const size_t have = p->rxOffset;
    const unsigned b0 = have > 0u ? (unsigned)p->rx[0] : 0u;
    const unsigned b1 = have > 1u ? (unsigned)p->rx[1] : 0u;
    const unsigned b2 = have > 2u ? (unsigned)p->rx[2] : 0u;
    const unsigned b3 = have > 3u ? (unsigned)p->rx[3] : 0u;
    const unsigned b4 = have > 4u ? (unsigned)p->rx[4] : 0u;
    size_t declared = have >= 2u ? 2u + (((size_t)b0 << 8) | b1) : 0u;
    LOG_TRACE("PnaTms: FRAME DIAG %s proto=%d stage=%d op=%llu "
              "received=%lu expected=%lu declared=%lu "
              "first5=%02X %02X %02X %02X %02X",
              reason, protoResult, (int)p->stage,
              (unsigned long long)p->operationId,
              (unsigned long)have, (unsigned long)p->rxLength,
              (unsigned long)declared, b0, b1, b2, b3, b4);
    if (have >= PNA_TMS_END_SIZE && have == p->rxLength &&
        have <= sizeof(p->rx)) {
        uint8_t calculatedLrc = 0u;
        for (size_t i = 2u; i + 1u < have; ++i)
            calculatedLrc ^= p->rx[i];
        LOG_TRACE("PnaTms: FRAME DIAG tail SO=%02X LRC=%02X calculated=%02X",
                  (unsigned)p->rx[have - 2u],
                  (unsigned)p->rx[have - 1u], (unsigned)calculatedLrc);
    }
}
/* Debug identity quality without exposing serial/terminal identifiers. */
static void logIdentityField(const char* name, const uint8_t* bytes, size_t len) {
    size_t nonblank = 0u;
    size_t nulCount = 0u;
    size_t nonAscii = 0u;
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = bytes[i];
        if (b != (uint8_t)' ' && b != 0u) ++nonblank;
        if (b == 0u) ++nulCount;
        if (b > 0x7eu || (b < 0x20u && b != 0u)) ++nonAscii;
    }
    LOG_TRACE("PnaTms: ID field=%s width=%lu nonblank=%lu nul=%lu nonAscii=%lu",
              name, (unsigned long)len, (unsigned long)nonblank,
              (unsigned long)nulCount, (unsigned long)nonAscii);
}

static void logResumeIdentityQuality(const PnaTmsIdentity* id) {
    if (!id) return;
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
    if (!error) return;
    LOG_TRACE("PnaTms: SERVER ERROR dataLenField=%c%c actualMsgBytes=%lu",
              (char)error->lengthField[0], (char)error->lengthField[1],
              (unsigned long)error->messageLength);
    for (size_t off = 0; off < error->messageLength; off += 16u) {
        char line[16u * 3u + 1u];
        size_t used = 0u;
        size_t end = error->messageLength - off;
        if (end > 16u) end = 16u;
        for (size_t j = 0; j < end; ++j) {
            int n = snprintf(line + used, sizeof(line) - used, "%02X ",
                             (unsigned)error->message[off + j]);
            if (n < 0 || (size_t)n >= sizeof(line) - used) break;
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
    uint64_t fraction = p->totalBytes
                            ? (p->downloadedBytes * 100u) / p->totalBytes : 0;
    uint8_t percent = (uint8_t)(fraction > 99u ? 99u : fraction);
#if PNA_TMS_ENABLE_DIAGNOSTICS
    /* Do not show 100% merely because the last sector was received. */
    if ((p->cfg.checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
         p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY ||
         p->cfg.checkpoint == PNA_TMS_CHECKPOINT_REVERIFY_STAGED) &&
        state == TMS_STATE_VERIFYING && p->fileCount != 0u &&
        p->fileIndex + 1u == p->fileCount &&
        p->verifiedBytes == p->totalBytes)
        percent = 100u;
#endif
    TmsBackendEvent ev = {
        .type = TMS_BACKEND_EVENT_PROGRESS,
        .state = state,
        .progress = percent,
        .fileIndex = (uint16_t)(p->fileCount ? p->fileIndex + 1 : 0),
        .fileCount = (uint16_t)p->fileCount
    };
    if (ev.fileIndex > ev.fileCount)
        ev.fileIndex = ev.fileCount;
    p->notify(p->notifyContext, id, &ev);
    return p->active && p->operationId == id;
}

static void releaseNth(PnaTms* p) {
    if (!p->tx)
        return;
    /* NTH callbacks may no longer reference the backend after release. */
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
    p->frameReady = false;
    p->frameBad = false;
    p->netFailed = false;
}

static bool closeFile(PnaTms* p, bool durable) {
    if (!p->opened)
        return true;
    FileHandle* opened = p->opened;
    p->opened = NULL;
    bool ok = true;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    const bool traceVerify = p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE &&
                             p->stage == PNA_TMS_VERIFY_FILE;
#endif
    if (durable) {
#if PNA_TMS_ENABLE_DIAGNOSTICS
        if (traceVerify) LOG_TRACE("PnaTms: STAGE5 flush begin");
#endif
        int ret = OOP_CALL(file(), flush, opened);
#if PNA_TMS_ENABLE_DIAGNOSTICS
        if (traceVerify) LOG_TRACE("PnaTms: STAGE5 flush end ret=%d", ret);
#endif
        if (ret != 0) ok = false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
        if (traceVerify) LOG_TRACE("PnaTms: STAGE5 sync begin");
#endif
        ret = OOP_CALL(file(), sync, opened);
#if PNA_TMS_ENABLE_DIAGNOSTICS
        if (traceVerify) LOG_TRACE("PnaTms: STAGE5 sync end ret=%d", ret);
#endif
        if (ret != 0) ok = false;
    }
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (traceVerify) LOG_TRACE("PnaTms: STAGE5 close begin");
#endif
    int ret = OOP_CALL(file(), close, opened);
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (traceVerify) LOG_TRACE("PnaTms: STAGE5 close end ret=%d", ret);
#endif
    if (ret != 0) ok = false;
    return ok;
}

static bool discardStaging(PnaTms* p) {
    bool ok = closeFile(p, false);
    for (size_t i = 0; i < p->stagedCount; ++i) {
        if (p->stagedPaths[i][0]) {
            int ret = OOP_CALL(file(), remove, p->stagedPaths[i]);
            LOG_TRACE("PnaTms: STORAGE cleanup path='%s' removeRet=%d",
                      p->stagedPaths[i], ret);
            if (ret != 0 && OOP_CALL(file(), exists, p->stagedPaths[i]))
                ok = false;
        }
    }
    p->stagedCount = 0;
    memset(p->stagedPaths, 0, sizeof(p->stagedPaths));
    return ok;
}

/* All resources are released BEFORE notifying TmsMgr. */
static void finish(PnaTms* p, TmsResult result, TmsError error, int code) {
    if (!p->active) return;
    p->active = false;
    p->stage = PNA_TMS_IDLE;
    releaseNth(p);
    (void)closeFile(p, false);
    if (result != TMS_RESULT_SUCCESS && result != TMS_RESULT_NO_UPDATE) {
        bool cleaned = discardStaging(p);
#if !PNA_TMS_ENABLE_DIAGNOSTICS
        (void)cleaned;
#endif
#if PNA_TMS_ENABLE_DIAGNOSTICS
        if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY &&
            p->stage8JournalStarted) {
            /* Keep incomplete journal if cleanup failed, so reboot recovery
             * can retry deletion instead of orphaning partial files. */
            if (cleaned && !pnaTmsStageDiscard(p->cfg.stagingPrefix, false))
                LOG_TRACE("PnaTms: STAGE8 journal cleanup failed; recover on reboot");
            else if (!cleaned)
                LOG_TRACE("PnaTms: STAGE8 partial-file cleanup incomplete; "
                          "journal retained for recovery");
            p->stage8JournalStarted = false;
        }
#endif
    }
    /* On success the installer owns staging lifecycle; do not unlink files. */
    TmsBackendNotify callback = p->notify;
    void* notifyContext = p->notifyContext;
    uint64_t id = p->operationId;
    p->notify = NULL;
    p->notifyContext = NULL;
    TmsBackendEvent ev = {
        .type = TMS_BACKEND_EVENT_DONE,
        .result = result,
        .error = error,
        .backendCode = code
    };
    LOG_TRACE("PnaTms: complete id=%llu result=%d err=%d code=%d",
              (unsigned long long)id, (int)result, (int)error, code);
    if (callback) callback(notifyContext, id, &ev);
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
    p->rxOffset = 0;
    p->rxLength = 2;
    p->frameReady = false;
    p->frameBad = false;
    p->netFailed = false;
}

/* ---------- NTH callback integration, directly in the vendor backend ---------- */
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
        p->netError = tx->lastError;
        p->netFailed = true;
        LOG_TRACE("PnaTms: NTH failure deferred op=%llu stage=%d nthErr=%d nthState=%d",
                  (unsigned long long)p->operationId, (int)p->stage,
                  (int)p->netError, (int)tx->state);
    }
    /* Never release an NTH transaction inside its own callback. */
    return 0;
}

static int8_t onNthTimeout(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (p && p->active && p->tx == tx) {
        p->netError = NTH_ERR_TIMEOUT;
        p->netFailed = true;
    }
    return 0;
}

static size_t nthReceiveLimit(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (!p || !p->active || p->tx != tx || p->frameBad ||
        p->rxLength > sizeof(p->rx) ||
        p->rxOffset > sizeof(p->rx) ||
        p->rxOffset >= p->rxLength)
        return 0;
    return p->rxLength - p->rxOffset;
}

static void onNthChunk(NthTransaction* tx, ByteArray* chunk, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (!p || p->tx != tx || !p->active || !chunk || !chunk->data)
        return;
    /* Preserve Stage 7's defensive size checks without the high-volume
     * temporary RX trace. The intermittent device crash remains unproven. */
    if (p->frameBad || p->rxLength > sizeof(p->rx) ||
        p->rxOffset > sizeof(p->rx) || p->rxOffset > p->rxLength ||
        chunk->len > sizeof(p->rx) - p->rxOffset ||
        chunk->len > p->rxLength - p->rxOffset) {
        LOG_TRACE("PnaTms: RX bounds error offset=%lu len=%lu expected=%lu capacity=%lu",
                  (unsigned long)p->rxOffset, (unsigned long)chunk->len,
                  (unsigned long)p->rxLength, (unsigned long)sizeof(p->rx));
        p->frameBad = true;
        return;
    }
    memcpy(p->rx + p->rxOffset, chunk->data, chunk->len);
    p->rxOffset += chunk->len;
    if (p->rxLength == 2 && p->rxOffset == 2) {
        size_t expected = 0;
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
                      (unsigned long long)p->operationId,
                      (unsigned)p->rx[0], (unsigned)p->rx[1],
                      (unsigned long)expected);
#endif
        }
    }
}

static NthRxDecision nthIsComplete(NthTransaction* tx, void* user) {
    PnaTms* p = (PnaTms*)user;
    if (!p || p->tx != tx || !p->active || p->frameBad ||
        p->rxLength > sizeof(p->rx) || p->rxOffset > sizeof(p->rx))
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
    p->tx->keepAlive = true;
    p->tx->streamRx = true;
    p->tx->rxTimeoutInactivity = true;
    p->tx->timeoutMs = p->cfg.ioTimeoutMs ? p->cfg.ioTimeoutMs
                                         : NT_DEFAULT_TIMEOUT_MS;
    p->tx->userData = p;
    p->tx->onConnect = onNthConnect;
    p->tx->onReceive = onNthReceive;
    p->tx->onFailure = onNthFailure;
    p->tx->onTimeout = onNthTimeout;
    p->tx->onChunk = onNthChunk;
    p->tx->isComplete = nthIsComplete;
    p->tx->receiveLimit = nthReceiveLimit;
    NthResult result = nth()->connect(p->tx, p->cfg.host, p->cfg.port);
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
        (p->tx->state != NTH_TX_READY &&
         p->tx->state != NTH_TX_COMPLETED))
        return false;
    resetResponse(p);
    ByteArray ba = {.data = (uint8_t*)bytes, .len = len, .capacity = len};
    if (nth()->send(p->tx, &ba) != NTH_OK)
        return false;
    p->stage = PNA_TMS_WAIT_RESPONSE;
    return true;
}

/* The legacy client sends sector-request filenames padded with spaces,
 * even when the manifest contains a shorter NUL-terminated filename.
 * Preserve this compatibility behavior without treating the 16-byte
 * manifest field as an unbounded C string. */
static void makeLegacyRequestName(const uint8_t manifestName[16],
                                  uint8_t requestName[16]) {
    memset(requestName, (uint8_t)' ', PNA_TMS_FILE_NAME_SIZE);
    for (size_t i = 0; i < PNA_TMS_FILE_NAME_SIZE; ++i) {
        if (manifestName[i] == 0u)
            break;
        requestName[i] = manifestName[i];
    }
}

static bool sendRequest(PnaTms* p) {
    size_t len = 0;
    PnaProtoResult proto;
    if (!p->manifestReceived) {
        if (!reportProgress(p, TMS_STATE_CHECKING)) return false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
        logResumeIdentityQuality(&p->cfg.identity);
#endif
        proto = pnaTmsBuildResume(&p->cfg.identity, p->request,
                                  sizeof(p->request), &len);
        LOG_TRACE("PnaTms: build command 10 op=%llu bytes=%lu protoResult=%d",
                  (unsigned long long)p->operationId,
                  (unsigned long)len, (int)proto);
    } else {
        if (p->fileIndex >= p->fileCount ||
            !reportProgress(p, TMS_STATE_DOWNLOADING)) return false;
        uint8_t wireName[PNA_TMS_FILE_NAME_SIZE];
        makeLegacyRequestName(p->files[p->fileIndex].name, wireName);
        proto = pnaTmsBuildSectorRequest(wireName, p->sectorIndex,
                                          p->request,
                                          sizeof(p->request), &len);
    }
    if (!p->active) return false;
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
    } else if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR &&
               p->manifestReceived) {
        LOG_TRACE("PnaTms: command 12 queued op=%llu sector=%u bytes=%lu",
                  (unsigned long long)p->operationId,
                  (unsigned)p->sectorIndex, (unsigned long)len);
    }
#endif
    return true;
}

static bool beginFile(PnaTms* p) {
    if (p->fileIndex >= p->fileCount) return false;
    const size_t i = p->fileIndex;
    int result = snprintf(p->stagedPaths[i], sizeof(p->stagedPaths[i]),
                          "%s_%02u.bin", p->cfg.stagingPrefix, (unsigned)i);
    if (result <= 0 || (size_t)result >= sizeof(p->stagedPaths[i])) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return false;
    }
    /* File names from the network are NEVER used as filesystem paths. */
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE ||
        (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE ||
         p->cfg.checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 ||
         p->cfg.checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
         p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY)) {
        /* A diagnostic must NEVER truncate a preexisting file. A caller must
         * supply a distinct scratch prefix, and the File HAL must implement
         * exists() reliably. The same check applies after deletion. */
        if (OOP_CALL(file(), exists, p->stagedPaths[i])) {
            LOG_TRACE("PnaTms: diagnostic scratch file exists, refusing overwrite");
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return false;
        }
        LOG_TRACE("PnaTms: scratch file create path='%s'", p->stagedPaths[i]);
    }
#endif
    p->stagedCount = i + 1;
    p->opened = OOP_CALL(file(), open, p->stagedPaths[i], "wb+");
    if (!p->opened) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return false;
    }
    p->sectorIndex = 0;
    p->sectorAttempt = 0;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY) {
        p->diagnosticCrc = UINT32_C(0xffffffff);
        p->verifyStarted = false;
        p->verifyOffset = 0u;
    }
#endif
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
    LOG_TRACE("PnaTms: sector retry %u, attempt %u",
              (unsigned)p->sectorIndex, (unsigned)(p->sectorAttempt + 1));
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
              (unsigned long long)p->operationId,
              (unsigned long)p->fileCount,
              (unsigned long long)p->totalBytes);
    for (size_t i = 0; i < p->fileCount; ++i) {
        char name[PNA_TMS_FILE_NAME_SIZE + 1u];
        char rawDigestHex[PNA_TMS_MD5_SIZE * 2u + 1u];
        const char* digits = "0123456789ABCDEF";
        size_t len = PNA_TMS_FILE_NAME_SIZE;
        for (size_t j = 0; j < PNA_TMS_FILE_NAME_SIZE; ++j) {
            uint8_t ch = p->files[i].name[j];
            name[j] = (ch >= 0x20u && ch <= 0x7eu) ? (char)ch : '?';
        }
        while (len && name[len - 1u] == ' ') --len;
        name[len] = '\0';
        /* Hex display of raw field, not an assumption about its MD5 format. */
        for (size_t j = 0; j < PNA_TMS_MD5_SIZE; ++j) {
            rawDigestHex[j * 2u] = digits[p->files[i].md5[j] >> 4u];
            rawDigestHex[j * 2u + 1u] = digits[p->files[i].md5[j] & 0x0fu];
        }
        rawDigestHex[PNA_TMS_MD5_SIZE * 2u] = '\0';
        LOG_TRACE("PnaTms: file[%lu] name='%s' size=%lu md5RawHex=%s",
                  (unsigned long)i, name,
                  (unsigned long)p->files[i].size, rawDigestHex);
    }
}
#endif


#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Manifest metadata is fixed-width ASCII, not C strings.
 * Print only numeric fields; never log terminal identity, filename or digest.
 */
static void logManifestNumericField(const char* label,
                                    const uint8_t* data, size_t length) {
    static const char hexDigits[] = "0123456789ABCDEF";
    char ascii[9];
    char hex[8u * 3u + 1u];
    bool allDigits = true;

    if (!label || !data || length == 0u || length > 8u)
        return;

    for (size_t i = 0u; i < length; ++i) {
        const uint8_t ch = data[i];
        ascii[i] = (ch >= 0x20u && ch <= 0x7eu) ? (char)ch : '?';
        if (ch < '0' || ch > '9')
            allDigits = false;
        hex[3u * i] = hexDigits[ch >> 4u];
        hex[3u * i + 1u] = hexDigits[ch & 0x0fu];
        hex[3u * i + 2u] = ' ';
    }
    ascii[length] = '\0';
    hex[3u * length] = '\0';
    LOG_TRACE("PnaTms: MANIFEST FIELD %s ascii='%s' hex=%s digits=%d",
              label, ascii, hex, (int)allDigits);
}

static void logManifestFieldFailure(const PnaTms* p) {
    if (!p || p->rxLength < 9u || p->rxLength > sizeof(p->rx))
        return;

    LOG_TRACE("PnaTms: MANIFEST FIELD frameBytes=%lu inferredRecords=%lu maxSize=%lu",
              (unsigned long)p->rxLength,
              (unsigned long)((p->rxLength - 9u) / 40u),
              (unsigned long)PNA_TMS_MAX_FILE_SIZE);
    logManifestNumericField("count", p->rx + 5u, 2u);

    if (p->rxLength >= 49u) {
        /* First record starts at offset 7. Filename occupies 16 bytes. */
        logManifestNumericField("file[0].size", p->rx + 23u, 8u);
    }
}
#endif

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Stage 7: validate every destination BEFORE creating the first file.
 * The File HAL exposes no capacity query in the supplied interface, so a
 * platform-specific optional callback may enforce available free space.
 * A path is marked as owned only after beginFile() creates it; a failed
 * preflight must never delete a preexisting file. */
static bool preflightAllFiles(PnaTms* p) {
    for (size_t i = 0u; i < p->fileCount; ++i) {
        char path[PNA_TMS_STAGE_PATH_SIZE];
        int n = snprintf(path, sizeof(path), "%s_%02u.bin",
                         p->cfg.stagingPrefix, (unsigned)i);
        if (n <= 0 || (size_t)n >= sizeof(path)) {
            LOG_TRACE("PnaTms: STAGE7 staging path too long file=%lu",
                      (unsigned long)i);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return false;
        }
        if (OOP_CALL(file(), exists, path)) {
            LOG_TRACE("PnaTms: STAGE7 destination already exists file=%lu path='%s'",
                      (unsigned long)i, path);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return false;
        }
    }
    if (p->cfg.checkStorageSpace) {
        if (!p->cfg.checkStorageSpace(p->cfg.storageSpaceContext,
                                      p->cfg.stagingPrefix, p->totalBytes)) {
            LOG_TRACE("PnaTms: STAGE7 insufficient/unverified capacity "
                      "requiredBytes=%llu",
                      (unsigned long long)p->totalBytes);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return false;
        }
        LOG_TRACE("PnaTms: STAGE7 capacity preflight PASS requiredBytes=%llu",
                  (unsigned long long)p->totalBytes);
    } else {
        LOG_TRACE("PnaTms: STAGE7 capacity preflight unavailable "
                  "requiredBytes=%llu (File HAL has no free-space query)",
                  (unsigned long long)p->totalBytes);
    }
    return true;
}
#endif

static void handleManifest(PnaTms* p) {
    size_t count = 0;
    PnaProtoResult parsed = pnaTmsParseManifest(
        p->rx, p->rxLength, p->files, PNA_TMS_MAX_FILES, &count);
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
    p->fileCount = count;
    p->fileIndex = 0;
    p->totalBytes = 0;
    for (size_t i = 0; i < count; ++i)
        p->totalBytes += p->files[i].size;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANIFEST) {
        /* Stop before beginFile(): zero storage and installation side effects. */
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
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE) {
        LOG_TRACE("PnaTms: STAGE5 download file 1/%lu size=%lu "
                  "(no package verification or installation)",
                  (unsigned long)p->fileCount,
                  (unsigned long)p->files[0].size);
    }
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5) {
        LOG_TRACE("PnaTms: STAGE6 download file 1/%lu size=%lu "
                  "(incremental CRC32 + manifest MD5; no installation)",
                  (unsigned long)p->fileCount,
                  (unsigned long)p->files[0].size);
    }
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY) {
        const bool managed = p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY;
        if (managed) {
            /* A previous READY package belongs to the application until it
             * explicitly discards it. Only a valid INCOMPLETE journal can
             * authorize cleanup of interrupted staging files. */
            PnaTmsStageStatus previous =
                pnaTmsStageInspect(p->cfg.stagingPrefix, NULL);
            if (previous == PNA_TMS_STAGE_INCOMPLETE) {
                LOG_TRACE("PnaTms: STAGE8 recovering incomplete staging");
                if (!pnaTmsStageDiscard(p->cfg.stagingPrefix, false)) {
                    fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                    return;
                }
            } else if (previous != PNA_TMS_STAGE_EMPTY) {
                LOG_TRACE("PnaTms: STAGE8 destination not empty status=%d",
                          (int)previous);
                fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                return;
            }
        }
        LOG_TRACE("PnaTms: STAGE%d download %lu files totalBytes=%llu "
                  "(all-file MD5; no installation)",
                  managed ? 8 : 7, (unsigned long)p->fileCount,
                  (unsigned long long)p->totalBytes);
        if (!preflightAllFiles(p)) return;
        if (managed) {
            if (!pnaTmsStageBegin(p->cfg.stagingPrefix, p->files,
                                  p->fileCount, p->totalBytes,
                                  &p->stage8Journal)) {
                LOG_TRACE("PnaTms: STAGE8 unable to create staging journal");
                fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                return;
            }
            p->stage8JournalStarted = true;
            LOG_TRACE("PnaTms: STAGE8 journal INCOMPLETE files=%lu bytes=%llu",
                      (unsigned long)p->fileCount,
                      (unsigned long long)p->totalBytes);
        }
    }
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE) {
        if (p->files[0].size < PNA_TMS_SECTOR_SIZE) {
            LOG_TRACE("PnaTms: STAGE4 requires a full first sector");
            fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_SECTOR);
            return;
        }
        LOG_TRACE("PnaTms: STAGE4 begin first file; will write exactly one sector");
        /* Unlike Stage 3, use the real beginFile()/handleSector() path. */
    }
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR) {
        /* Stay on the existing NTH connection. Do not call beginFile(),
         * which would create a staging file before requesting sector 0. */
        p->sectorIndex = 0u;
        p->sectorAttempt = 0u;
        LOG_TRACE("PnaTms: FIRST SECTOR request op=%llu file=1/%lu index=0 "
                  "fileSize=%lu",
                  (unsigned long long)p->operationId,
                  (unsigned long)p->fileCount,
                  (unsigned long)p->files[0].size);
        (void)sendRequest(p);
        return;
    }
#endif
    (void)beginFile(p);
}

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Complete Stage 4 using only File HAL. Read after close/reopen to check that
 * the newly downloaded 4096 bytes reached the backing file, not just a
 * buffered write. Never invoke installer or request the next sector. */
static void checkpointFirstSectorWrite(PnaTms* p, const PnaTmsSector* sector) {
    const size_t index = p->fileIndex;
    if (!p->opened || index >= p->stagedCount || !sector || !sector->bytes) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    const char* path = p->stagedPaths[index];
    if (!closeFile(p, true)) {
        LOG_TRACE("PnaTms: STAGE4 flush/sync/close failed");
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    p->opened = OOP_CALL(file(), open, path, "rb");
    if (!p->opened) {
        LOG_TRACE("PnaTms: STAGE4 reopen for read failed");
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    long actualSize = OOP_CALL(file(), size, p->opened);
    if (actualSize < 0 || (uint32_t)actualSize != sector->length) {
        LOG_TRACE("PnaTms: STAGE4 size mismatch actual=%ld expected=%u",
                  actualSize, (unsigned)sector->length);
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    uint8_t scratch[PNA_STAGE4_READ_CHUNK];
    size_t verified = 0u;
    while (verified < sector->length) {
        size_t chunk = (size_t)sector->length - verified;
        if (chunk > sizeof(scratch)) chunk = sizeof(scratch);
        if (OOP_CALL(file(), read, scratch, 1u, chunk, p->opened) != chunk ||
            memcmp(scratch, sector->bytes + verified, chunk) != 0) {
            LOG_TRACE("PnaTms: STAGE4 readback mismatch offset=%lu",
                      (unsigned long)verified);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        verified += chunk;
    }
    if (!closeFile(p, false)) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    if (OOP_CALL(file(), remove, path) != 0 ||
        OOP_CALL(file(), exists, path)) {
        LOG_TRACE("PnaTms: STAGE4 scratch cleanup failed");
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    /* This scratch file has been removed successfully. The diagnostic
     * completion path must not attempt another deletion. */
    p->stagedPaths[index][0] = '\0';
    p->stagedCount = 0u;
    LOG_TRACE("PnaTms: FIRST SECTOR WRITE verified op=%llu "
              "index=%u written=%u readback=%lu removed=1",
              (unsigned long long)p->operationId,
              (unsigned)sector->index, (unsigned)sector->length,
              (unsigned long)verified);
    finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
           (int)PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE);
}
#endif

static void handleSector(PnaTms* p) {
    PnaTmsSector sector;
    const PnaTmsFile* metadata = &p->files[p->fileIndex];
    uint32_t offset = (uint32_t)p->sectorIndex * PNA_TMS_SECTOR_SIZE;
    PnaProtoResult parsed = pnaTmsParseSector(p->rx, p->rxLength, &sector);
    if (parsed != PNA_PROTO_OK || offset >= metadata->size) {
        LOG_TRACE("PnaTms: sector invalid op=%llu proto=%d offset=%lu "
                  "fileSize=%lu frameBytes=%lu",
                  (unsigned long long)p->operationId, (int)parsed,
                  (unsigned long)offset, (unsigned long)metadata->size,
                  (unsigned long)p->rxLength);
        sectorRetry(p, TMS_ERR_PROTOCOL, PNA_DIAG_SECTOR);
        return;
    }
    uint32_t remaining = metadata->size - offset;
    uint16_t expected = remaining > PNA_TMS_SECTOR_SIZE
                            ? PNA_TMS_SECTOR_SIZE : (uint16_t)remaining;
    if (sector.index != p->sectorIndex || sector.length != expected) {
        LOG_TRACE("PnaTms: sector mismatch requested=%u received=%u "
                  "expectedBytes=%u receivedBytes=%u",
                  (unsigned)p->sectorIndex, (unsigned)sector.index,
                  (unsigned)expected, (unsigned)sector.length);
        sectorRetry(p, TMS_ERR_PROTOCOL, PNA_DIAG_SECTOR);
        return;
    }
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR) {
        /* At this point framing, LRC, sector number and byte count match
         * the manifest. The sector bytes are still only in the RX buffer.
         * DO NOT call File HAL or verification/installation hooks. */
        LOG_TRACE("PnaTms: FIRST SECTOR validated op=%llu index=%u "
                  "dataBytes=%u frameBytes=%lu expectedBytes=%u",
                  (unsigned long long)p->operationId,
                  (unsigned)sector.index, (unsigned)sector.length,
                  (unsigned long)p->rxLength, (unsigned)expected);
        finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
               (int)PNA_TMS_CHECKPOINT_FIRST_SECTOR);
        return;
    }
#endif
#if LONG_MAX < UINT32_MAX
    if (offset > (uint32_t)LONG_MAX) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
#endif
    /* Separate File HAL failures; previously both mapped to -107 without
     * enough information to distinguish seek, short write, or a null handle.
     * Do not retry a partially written sector without first auditing the
     * filesystem semantics.  Failure cleanup remains unchanged. */
    if (!p->opened) {
        LOG_TRACE("PnaTms: STORAGE no file handle file=%lu sector=%u "
                  "offset=%lu path='%s'",
                  (unsigned long)p->fileIndex, (unsigned)p->sectorIndex,
                  (unsigned long)offset, p->stagedPaths[p->fileIndex]);
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    int seekRet = OOP_CALL(file(), seek, p->opened, (long)offset,
                           FILE_SEEK_ORG_SET);
    if (seekRet != 0) {
        LOG_TRACE("PnaTms: STORAGE seek failed sector=%u offset=%lu "
                  "ret=%d path='%s'",
                  (unsigned)p->sectorIndex, (unsigned long)offset, seekRet,
                  p->stagedPaths[p->fileIndex]);
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    size_t wrote = OOP_CALL(file(), write, sector.bytes, 1u,
                            (size_t)sector.length, p->opened);
    if (wrote != (size_t)sector.length) {
        LOG_TRACE("PnaTms: STORAGE write failed sector=%u offset=%lu "
                  "requested=%u wrote=%lu downloaded=%llu path='%s'",
                  (unsigned)p->sectorIndex, (unsigned long)offset,
                  (unsigned)sector.length, (unsigned long)wrote,
                  (unsigned long long)p->downloadedBytes,
                  p->stagedPaths[p->fileIndex]);
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE) {
        /* The sector was written by the same File HAL calls as production.
         * Read back and remove the scratch file before reporting PASS. */
        checkpointFirstSectorWrite(p, &sector);
        return;
    }
#endif
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY)
        p->diagnosticCrc = stage5Crc32(p->diagnosticCrc,
                                       sector.bytes, sector.length);
#endif
    p->downloadedBytes += sector.length;
    p->sectorAttempt = 0;
    ++p->sectorIndex;
    if (!reportProgress(p, TMS_STATE_DOWNLOADING)) return;
    if ((uint32_t)p->sectorIndex * PNA_TMS_SECTOR_SIZE >= metadata->size)
        p->stage = PNA_TMS_VERIFY_FILE;
    else
        (void)sendRequest(p);
}

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Read back every byte after a durable close, then retain the scratch file
 * for explicit inspection. Do not invoke package.verify or the installer.
 * Partial/invalid files remain tracked and are deleted on failure. */
static void checkpointFullFile(PnaTms* p) {
    if (!p->opened || p->fileIndex >= p->stagedCount ||
        p->fileIndex >= p->fileCount) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    const size_t index = p->fileIndex;
    const uint32_t expected = p->files[index].size;
    const char* path = p->stagedPaths[index];
    LOG_TRACE("PnaTms: STAGE5 verify begin expected=%lu downloaded=%llu",
              (unsigned long)expected,
              (unsigned long long)p->downloadedBytes);
    if (!closeFile(p, true)) {
        LOG_TRACE("PnaTms: STAGE5 flush/sync/close failed");
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    LOG_TRACE("PnaTms: STAGE5 reopen read begin");
    p->opened = OOP_CALL(file(), open, path, "rb");
    LOG_TRACE("PnaTms: STAGE5 reopen read end ok=%d",
              p->opened != NULL);
    if (!p->opened) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    LOG_TRACE("PnaTms: STAGE5 size query begin");
    long actual = OOP_CALL(file(), size, p->opened);
    LOG_TRACE("PnaTms: STAGE5 size query end actual=%ld expected=%lu",
              actual, (unsigned long)expected);
    if (actual < 0 || (uint64_t)actual != (uint64_t)expected) {
        LOG_TRACE("PnaTms: STAGE5 size mismatch actual=%ld expected=%lu",
                  actual, (unsigned long)expected);
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    uint8_t bytes[PNA_STAGE5_READ_CHUNK];
    uint32_t crc = UINT32_C(0xffffffff);
    uint32_t verified = 0u;
    LOG_TRACE("PnaTms: STAGE5 readback begin chunk=%u",
              (unsigned)sizeof(bytes));
    while (verified < expected) {
        size_t n = (size_t)(expected - verified);
        if (n > sizeof(bytes)) n = sizeof(bytes);
        if (OOP_CALL(file(), read, bytes, 1u, n, p->opened) != n) {
            LOG_TRACE("PnaTms: STAGE5 readback failed offset=%lu",
                      (unsigned long)verified);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        crc = stage5Crc32(crc, bytes, n);
        verified += (uint32_t)n;
        /* One log per 64 KiB, plus final partial block. */
        if ((verified % (64u * 1024u)) == 0u || verified == expected) {
            LOG_TRACE("PnaTms: STAGE5 readback progress=%lu/%lu",
                      (unsigned long)verified, (unsigned long)expected);
        }
    }
    LOG_TRACE("PnaTms: STAGE5 readback end verified=%lu",
              (unsigned long)verified);
    if (!closeFile(p, false)) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    if (crc != p->diagnosticCrc) {
        LOG_TRACE("PnaTms: STAGE5 readback CRC32 mismatch fileIndex=%lu",
                  (unsigned long)index);
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    LOG_TRACE("PnaTms: FULL FILE PASS op=%llu bytes=%lu "
              "sectors=%lu crc32=%08lX staged='%s' retained=1",
              (unsigned long long)p->operationId,
              (unsigned long)verified,
              (unsigned long)((expected + PNA_TMS_SECTOR_SIZE - 1u) /
                              PNA_TMS_SECTOR_SIZE),
              (unsigned long)(crc ^ UINT32_C(0xffffffff)), path);
    /* Disown this deliberately retained file BEFORE diagnostic completion.
     * No other files were opened at this checkpoint. The next attempt must
     * use another path or explicitly remove the retained file first. */
    p->stagedPaths[index][0] = '\0';
    p->stagedCount = 0u;
    finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
           (int)PNA_TMS_CHECKPOINT_FULL_FILE);
}

/*
 * Stage 6 checkpoint: execute a bounded amount of readback work per process()
 * call. Flash flush/sync/open/close calls can still block individually;
 * cooperative verification only guarantees a bound on read operations.
 *
 * The server's MD5 is an untrusted integrity signal, NOT proof of origin.
 * Signature/authenticity verification is required before installation.
 */
static void checkpointIncrementalMd5(PnaTms* p) {
    if (!p || p->fileIndex >= p->stagedCount ||
        p->fileIndex >= p->fileCount) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    const size_t index = p->fileIndex;
    const uint32_t expected = p->files[index].size;
    const char* path = p->stagedPaths[index];
    const bool managed = p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY;
    const bool allFiles = managed ||
        p->cfg.checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5;
    const int stageNumber = managed ? 8 : (allFiles ? 7 : 6);
    if (!p->verifyStarted) {
        /* Stage 6 downloads only file 0; Stage 7 downloads all files.
         * Verify the CURRENT file, not the cumulative download total. */
        if (!p->opened || p->downloadedBytes < p->verifiedBytes ||
            p->downloadedBytes - p->verifiedBytes != expected) {
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        LOG_TRACE("PnaTms: STAGE%d verify file=%lu/%lu bytes=%lu",
                  stageNumber, (unsigned long)(index + 1u),
                  (unsigned long)p->fileCount, (unsigned long)expected);
        if (!closeFile(p, true)) {
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        p->opened = OOP_CALL(file(), open, path, "rb");
        if (!p->opened) {
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        long actual = OOP_CALL(file(), size, p->opened);
        if (actual < 0 || (uint64_t)actual != (uint64_t)expected) {
            LOG_TRACE("PnaTms: STAGE6 size mismatch expected=%lu actual=%ld",
                      (unsigned long)expected, actual);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        p->verifyOffset = 0u;
        p->verifyCrc = UINT32_C(0xffffffff);
        pnaTmsMd5Init(&p->verifyMd5);
        p->verifyStarted = true;
        /* Do not read back the entire file on this first tick. */
        return;
    }
    /* Reuse the backend's existing frame storage: no new stack/heap buffer.
     * NTH has completed the last sector response before VERIFY_FILE starts.
     */
    uint8_t* bytes = p->rx;
    const size_t chunkCapacity = sizeof(p->rx) < PNA_STAGE6_READ_CHUNK
                                     ? sizeof(p->rx) : PNA_STAGE6_READ_CHUNK;
    const uint32_t tickStart = NTH_GET_TICK();
    for (unsigned reads = 0u;
         reads < PNA_STAGE6_READS_PER_TICK && p->verifyOffset < expected;
         ++reads) {
        size_t count = (size_t)(expected - p->verifyOffset);
        if (count > chunkCapacity) count = chunkCapacity;
        if (OOP_CALL(file(), read, bytes, 1u, count, p->opened) != count) {
            LOG_TRACE("PnaTms: STAGE%d read failure file=%lu offset=%lu",
                      stageNumber, (unsigned long)(index + 1u),
                      (unsigned long)p->verifyOffset);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        p->verifyCrc = stage5Crc32(p->verifyCrc, bytes, count);
        pnaTmsMd5Update(&p->verifyMd5, bytes, count);
        p->verifyOffset += (uint32_t)count;
        /* The File HAL call itself may block; this prevents long batches,
         * not blocking inside an individual driver call.
         */
        if ((uint32_t)(NTH_GET_TICK() - tickStart) >=
            PNA_STAGE6_TICK_BUDGET_MS)
            break;
    }
    if (p->verifyOffset < expected) {
        if ((p->verifyOffset % (64u * 1024u)) == 0u)
            LOG_TRACE("PnaTms: STAGE%d readback file=%lu/%lu %lu/%lu",
                      stageNumber,
                      (unsigned long)(index + 1u),
                      (unsigned long)p->fileCount,
                      (unsigned long)p->verifyOffset,
                      (unsigned long)expected);
        return;
    }
    uint8_t calculatedMd5[PNA_TMS_MD5_SIZE];
    pnaTmsMd5Final(&p->verifyMd5, calculatedMd5);
    bool crcOk = p->verifyCrc == p->diagnosticCrc;
    bool md5Ok = memcmp(calculatedMd5, p->files[index].md5,
                        sizeof(calculatedMd5)) == 0;
    if (!closeFile(p, false)) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    LOG_TRACE("PnaTms: STAGE%d readback file=%lu/%lu bytes=%lu "
              "CRC32=%08lX match=%u manifestMD5Match=%u",
              stageNumber,
              (unsigned long)(index + 1u), (unsigned long)p->fileCount,
              (unsigned long)p->verifyOffset,
              (unsigned long)(p->verifyCrc ^ UINT32_C(0xffffffff)),
              (unsigned)crcOk, (unsigned)md5Ok);
    if (!md5Ok) {
        char actualHex[33], manifestHex[33];
        static const char digits[] = "0123456789ABCDEF";
        for (size_t i = 0u; i < PNA_TMS_MD5_SIZE; ++i) {
            actualHex[2u * i] = digits[calculatedMd5[i] >> 4u];
            actualHex[2u * i + 1u] = digits[calculatedMd5[i] & 0x0fu];
            manifestHex[2u * i] = digits[p->files[index].md5[i] >> 4u];
            manifestHex[2u * i + 1u] = digits[p->files[index].md5[i] & 0x0fu];
        }
        actualHex[32] = '\0';
        manifestHex[32] = '\0';
        LOG_TRACE("PnaTms: STAGE%d MD5 mismatch file=%lu computed=%s manifestRaw=%s",
                  stageNumber, (unsigned long)(index + 1u),
                  actualHex, manifestHex);
    }
    if (!crcOk || !md5Ok) {
        fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
        return;
    }
    LOG_TRACE("PnaTms: STAGE%d FILE VERIFIED file=%lu/%lu op=%llu "
              "path='%s' bytes=%lu install=0",
              stageNumber, (unsigned long)(index + 1u),
              (unsigned long)p->fileCount,
              (unsigned long long)p->operationId, path,
              (unsigned long)expected);
    if (allFiles) {
        if (managed && !pnaTmsStageMarkVerified(p->cfg.stagingPrefix,
                                                &p->stage8Journal,
                                                index + 1u)) {
            LOG_TRACE("PnaTms: STAGE8 failed to commit verified file=%lu",
                      (unsigned long)(index + 1u));
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        p->verifiedBytes += expected;
        p->verifyStarted = false;
        p->verifyOffset = 0u;
        if (index + 1u < p->fileCount) {
            /* Keep earlier verified files under cleanup ownership until
             * ALL files have been successfully verified. */
            afterFile(p);
            return;
        }
        if (p->verifiedBytes != p->totalBytes) {
            fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
            return;
        }
        /* Every file is verified. Report 100% before diagnostic DONE.
         * Disarm auto-cleanup ONLY on full success, retaining all files. */
        if (!reportProgress(p, TMS_STATE_VERIFYING)) return;
        if (managed) {
            if (!pnaTmsStageMarkReady(p->cfg.stagingPrefix,
                                      &p->stage8Journal)) {
                LOG_TRACE("PnaTms: STAGE8 READY journal commit failed");
                fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                return;
            }
            /* Persist READY before announcing completion; never install. */
            p->stage8JournalStarted = false;
            LOG_TRACE("PnaTms: STAGE8 READY_TO_INSTALL files=%lu bytes=%llu "
                      "journal=1 install=0 publisherAuth=0",
                      (unsigned long)p->fileCount,
                      (unsigned long long)p->verifiedBytes);
        } else {
            LOG_TRACE("PnaTms: STAGE7 ALL FILES VERIFIED files=%lu bytes=%llu "
                      "retained=1 install=0",
                      (unsigned long)p->fileCount,
                      (unsigned long long)p->verifiedBytes);
        }
        p->stagedCount = 0u;
        memset(p->stagedPaths, 0, sizeof(p->stagedPaths));
        finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
               (int)(managed ? PNA_TMS_CHECKPOINT_MANAGED_READY :
                               PNA_TMS_CHECKPOINT_ALL_FILES_MD5));
        return;
    }
    /* Stage 6 preserves its previous single-file diagnostic behavior. */
    p->stagedPaths[index][0] = '\0';
    p->stagedCount = 0u;
    finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
           (int)PNA_TMS_CHECKPOINT_INCREMENTAL_MD5);
}
#endif

#if PNA_TMS_ENABLE_DIAGNOSTICS
/* Stage 9 is deliberately OFFLINE. Never register a network transaction,
 * change journal slots, delete staging, or use package/installer hooks.
 * Successful rehashing is valid for this invocation only: a later reboot
 * must rehash again, and MD5 alone is not publisher authentication.
 */
static void beginRestartReverification(PnaTms* p) {
    PnaTmsStageStatus status = pnaTmsStageInspect(p->cfg.stagingPrefix,
                                                  &p->stage8Journal);
    LOG_TRACE("PnaTms: STAGE9 journal inspection status=%d prefix='%s'",
              (int)status, p->cfg.stagingPrefix);
    if (status != PNA_TMS_STAGE_READY_UNTRUSTED) {
        /* Do not alter files/journal on missing, incomplete or corrupt data. */
        fail(p, status == PNA_TMS_STAGE_IO_ERROR ? TMS_ERR_STORAGE :
                TMS_ERR_INTEGRITY,
             status == PNA_TMS_STAGE_IO_ERROR ? PNA_DIAG_STORAGE :
                PNA_DIAG_VERIFY);
        return;
    }
    p->fileCount = p->stage8Journal.fileCount;
    p->totalBytes = p->stage8Journal.totalBytes;
    memcpy(p->files, p->stage8Journal.files,
           p->fileCount * sizeof(p->files[0]));
    p->fileIndex = 0u;
    p->verifiedBytes = 0u;
    p->downloadedBytes = 0u;
    p->verifyStarted = false;
    p->stage = PNA_TMS_VERIFY_FILE;
    LOG_TRACE("PnaTms: STAGE9 offline reverify start op=%llu files=%lu "
              "bytes=%llu network=0 install=0",
              (unsigned long long)p->operationId,
              (unsigned long)p->fileCount,
              (unsigned long long)p->totalBytes);
    (void)reportProgress(p, TMS_STATE_VERIFYING);
}

static void checkpointRestartReverify(PnaTms* p) {
    if (!p || p->fileIndex >= p->fileCount ||
        p->fileCount != p->stage8Journal.fileCount) {
        fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
        return;
    }
    const size_t index = p->fileIndex;
    const uint32_t expected = p->files[index].size;
    if (!p->verifyStarted) {
        char path[PNA_TMS_STAGE_PATH_SIZE];
        if (!pnaTmsStageFilePath(p->cfg.stagingPrefix, index,
                                 path, sizeof(path))) {
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        /* No path from the untrusted server manifest is used as a filename. */
        p->opened = OOP_CALL(file(), open, path, "rb");
        if (!p->opened) {
            LOG_TRACE("PnaTms: STAGE9 missing/unreadable file=%lu path='%s'",
                      (unsigned long)(index + 1u), path);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        const long actual = OOP_CALL(file(), size, p->opened);
        if (actual < 0 || (uint64_t)actual != expected) {
            LOG_TRACE("PnaTms: STAGE9 size mismatch file=%lu expected=%lu "
                      "actual=%ld", (unsigned long)(index + 1u),
                      (unsigned long)expected, actual);
            fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
            return;
        }
        p->verifyOffset = 0u;
        p->verifyCrc = UINT32_C(0xffffffff);
        pnaTmsMd5Init(&p->verifyMd5);
        p->verifyStarted = true;
        LOG_TRACE("PnaTms: STAGE9 file open file=%lu/%lu bytes=%lu",
                  (unsigned long)(index + 1u),
                  (unsigned long)p->fileCount, (unsigned long)expected);
        return; /* Avoid synchronous full-file hashing on the first call. */
    }
    const uint32_t tickStart = NTH_GET_TICK();
    for (unsigned reads = 0u;
         reads < PNA_STAGE6_READS_PER_TICK && p->verifyOffset < expected;
         ++reads) {
        size_t count = (size_t)(expected - p->verifyOffset);
        if (count > PNA_STAGE6_READ_CHUNK) count = PNA_STAGE6_READ_CHUNK;
        if (OOP_CALL(file(), read, p->rx, 1u, count, p->opened) != count) {
            LOG_TRACE("PnaTms: STAGE9 read failed file=%lu offset=%lu",
                      (unsigned long)(index + 1u),
                      (unsigned long)p->verifyOffset);
            fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
            return;
        }
        pnaTmsMd5Update(&p->verifyMd5, p->rx, count);
        p->verifyCrc = stage5Crc32(p->verifyCrc, p->rx, count);
        p->verifyOffset += (uint32_t)count;
        /* A single File HAL read can still block; only the batch is bounded. */
        if ((uint32_t)(NTH_GET_TICK() - tickStart) >=
            PNA_STAGE6_TICK_BUDGET_MS) break;
    }
    p->downloadedBytes = p->verifiedBytes + p->verifyOffset;
    if (p->verifyOffset < expected) {
        if ((p->verifyOffset % (64u * 1024u)) == 0u) {
            LOG_TRACE("PnaTms: STAGE9 readback file=%lu/%lu bytes=%lu/%lu",
                      (unsigned long)(index + 1u),
                      (unsigned long)p->fileCount,
                      (unsigned long)p->verifyOffset,
                      (unsigned long)expected);
            (void)reportProgress(p, TMS_STATE_VERIFYING);
        }
        return;
    }
    uint8_t computed[PNA_TMS_MD5_SIZE];
    pnaTmsMd5Final(&p->verifyMd5, computed);
    /* Detect a resize during readback. No journal or file is ever modified. */
    const long finalSize = OOP_CALL(file(), size, p->opened);
    const bool sizeOk = finalSize >= 0 && (uint64_t)finalSize == expected;
    const bool digestOk = memcmp(computed, p->files[index].md5,
                                 PNA_TMS_MD5_SIZE) == 0;
    const bool closeOk = closeFile(p, false);
    LOG_TRACE("PnaTms: STAGE9 rehash file=%lu/%lu bytes=%lu CRC32=%08lX "
              "sizeMatch=%u manifestMD5Match=%u",
              (unsigned long)(index + 1u),
              (unsigned long)p->fileCount,
              (unsigned long)expected,
              (unsigned long)(p->verifyCrc ^ UINT32_C(0xffffffff)),
              (unsigned)sizeOk, (unsigned)digestOk);
    if (!closeOk) {
        fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
        return;
    }
    if (!sizeOk || !digestOk) {
        fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
        return;
    }
    p->verifiedBytes += expected;
    p->downloadedBytes = p->verifiedBytes;
    p->fileIndex++;
    p->verifyStarted = false;
    p->verifyOffset = 0u;
    if (p->fileIndex < p->fileCount) {
        (void)reportProgress(p, TMS_STATE_VERIFYING);
        return;
    }
    if (p->verifiedBytes != p->totalBytes) {
        fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
        return;
    }
    /* reportProgress needs the last valid (zero-based) file index. */
    p->fileIndex = p->fileCount - 1u;
    if (!reportProgress(p, TMS_STATE_VERIFYING)) return;
    LOG_TRACE("PnaTms: STAGE9 ALL FILES REVERIFIED files=%lu bytes=%llu "
              "journalUnchanged=1 network=0 install=0 publisherAuth=0",
              (unsigned long)p->fileCount,
              (unsigned long long)p->verifiedBytes);
    finish(p, TMS_RESULT_DIAGNOSTIC, TMS_ERR_NONE,
           (int)PNA_TMS_CHECKPOINT_REVERIFY_STAGED);
}
#endif

static void handleFrame(PnaTms* p) {
    PnaTmsCommand command;
    PnaProtoResult validation = pnaTmsValidate(p->rx, p->rxLength, &command);
    if (validation != PNA_PROTO_OK) {
#if PNA_TMS_ENABLE_DIAGNOSTICS
        logFrameDiagnostic(p, "validation-failed", (int)validation);
#endif
        if (inSectorPhase(p)) sectorRetry(p, TMS_ERR_PROTOCOL, PNA_DIAG_FRAME);
        else fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_FRAME);
        return;
    }
    LOG_TRACE("PnaTms: received command=%d frameBytes=%lu op=%llu",
              (int)command, (unsigned long)p->rxLength,
              (unsigned long long)p->operationId);
    if (command == PNA_CMD_ERROR) {
        PnaTmsServerError serverError;
        if (pnaTmsParseError(p->rx, p->rxLength, &serverError) != PNA_PROTO_OK) {
            fail(p, TMS_ERR_PROTOCOL, PNA_DIAG_SERVER);
        } else {
            char message[sizeof(serverError.message) + 1u];
            for (size_t i = 0; i < serverError.messageLength; ++i) {
                uint8_t ch = serverError.message[i];
                message[i] = (ch >= 0x20u && ch <= 0x7eu) ? (char)ch : '?';
            }
            message[serverError.messageLength] = '\0';
            LOG_TRACE("PnaTms: server error msg='%s' bytes=%lu",
                      message, (unsigned long)serverError.messageLength);
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
    if (!p || !p->active) return;
    /* NTH is ticked by the application, not from this function. */
    for (unsigned transitions = 0;
         transitions < PNA_PUMP_BUDGET && p->active; ++transitions) {
        if (p->netFailed || (p->tx && p->tx->state == NTH_TX_FAILED)) {
            TmsError err = p->netError == NTH_ERR_TIMEOUT
                               ? TMS_ERR_TIMEOUT : (p->netError == NTH_ERR_PROTOCOL
                                 ? TMS_ERR_PROTOCOL : TMS_ERR_COMMUNICATION);
            LOG_TRACE("PnaTms: processing NTH failure op=%llu stage=%d error=%d",
                      (unsigned long long)p->operationId, (int)p->stage,
                      (int)p->netError);
            networkLost(p, err, (int)p->netError);
            return;
        }
        switch (p->stage) {
        case PNA_TMS_CONNECTING:
            if (!p->tx) { fail(p, TMS_ERR_COMMUNICATION, PNA_DIAG_CONNECT); return; }
            if (p->tx->state != NTH_TX_READY) return;
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
            if (!p->frameReady) return;
            p->frameReady = false;
            handleFrame(p);
            break;
        case PNA_TMS_VERIFY_FILE: {
#if PNA_TMS_ENABLE_DIAGNOSTICS
            if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_REVERIFY_STAGED) {
                checkpointRestartReverify(p);
                return;
            }
            if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 ||
                p->cfg.checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
                p->cfg.checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY) {
                if (!p->verifyStarted &&
                    !reportProgress(p, TMS_STATE_VERIFYING)) return;
                checkpointIncrementalMd5(p);
                return;
            }
#endif
            if (!reportProgress(p, TMS_STATE_VERIFYING)) return;
#if PNA_TMS_ENABLE_DIAGNOSTICS
            if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE) {
                checkpointFullFile(p);
                return;
            }
#endif
            const size_t index = p->fileIndex;
            if (!p->opened || index >= p->fileCount) {
                fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                return;
            }
            long sz = OOP_CALL(file(), size, p->opened);
            bool validSize = sz >= 0 && (uint64_t)sz == p->files[index].size;
            if (!closeFile(p, true) || !validSize) {
                fail(p, TMS_ERR_STORAGE, PNA_DIAG_STORAGE);
                return;
            }
            if (!p->cfg.package.verify(p->cfg.package.context,
                                       &p->files[index], p->stagedPaths[index])) {
                fail(p, TMS_ERR_INTEGRITY, PNA_DIAG_VERIFY);
                return;
            }
            afterFile(p);
            break;
        }
        case PNA_TMS_INSTALL_START: {
            if (!reportProgress(p, TMS_STATE_INSTALLING)) return;
            p->installationStarted = true;
            int ret = p->cfg.package.beginInstall(
                p->cfg.package.context, p->files,
                (const char (*)[PNA_TMS_STAGE_PATH_SIZE])p->stagedPaths,
                p->fileCount);
            if (ret < 0) fail(p, TMS_ERR_INSTALLATION, PNA_DIAG_INSTALL);
            else if (ret > 0) finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, 0);
            else p->stage = PNA_TMS_INSTALL_WAIT;
            return;
        }
        case PNA_TMS_INSTALL_WAIT: {
            int ret = p->cfg.package.pollInstall(p->cfg.package.context);
            if (ret < 0) fail(p, TMS_ERR_INSTALLATION, PNA_DIAG_INSTALL);
            else if (ret > 0) finish(p, TMS_RESULT_SUCCESS, TMS_ERR_NONE, 0);
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
    if (!p || !notify || p->active) return false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    /* A test checkpoint must never replace a real scheduled/automatic update. */
    if (p->cfg.checkpoint != PNA_TMS_CHECKPOINT_NONE &&
        trigger != TMS_TRIGGER_MANUAL)
        return false;
#else
    (void)trigger;
#endif
    /* Clean old completed staging before a new operation, not during install. */
    if (p->stagedCount) discardStaging(p);
    p->active = true;
    p->notify = notify;
    p->notifyContext = notifyContext;
    p->operationId = operationId;
    p->fileCount = 0;
    p->fileIndex = 0;
    p->sectorIndex = 0;
    p->sectorAttempt = 0;
    p->manifestReceived = false;
    p->totalBytes = 0;
    p->downloadedBytes = 0;
    p->installationStarted = false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    p->verifyStarted = false;
    p->verifyOffset = 0u;
    p->verifiedBytes = 0u;
    p->stage8JournalStarted = false;
#endif
    p->netError = NTH_OK;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (p->cfg.checkpoint == PNA_TMS_CHECKPOINT_REVERIFY_STAGED) {
        /* Stage 9 does not allocate an NTH transaction or contact the server. */
        beginRestartReverification(p);
        return true;
    }
#endif
    if (!reportProgress(p, TMS_STATE_CONNECTING)) return true;
    (void)connectNth(p); /* Error completion can be synchronous. */
    return true;
}

static bool cancelPna(void* context, uint64_t id) {
    PnaTms* p = (PnaTms*)context;
    if (!p || !p->active || p->operationId != id ||
        p->installationStarted || p->stage == PNA_TMS_INSTALL_START)
        return false;
    finish(p, TMS_RESULT_CANCELED, TMS_ERR_NONE, 0);
    return true;
}

bool pnaTmsInit(PnaTms* p, const PnaTmsConfig* cfg) {
    if (!p || !cfg || p->active) return false;
    /* Offline Stage 9 needs only the existing staging prefix. */
    bool offline = false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    offline = cfg->checkpoint == PNA_TMS_CHECKPOINT_REVERIFY_STAGED;
#endif
    if (!offline && (!memchr(cfg->host, '\0', sizeof(cfg->host)) ||
                     !cfg->host[0] || cfg->port == 0))
        return false;

    /* Diagnostic checkpoints do not install. Stage 4 uses only temporary File HAL
     * operations and needs a staging prefix, not verification/install hooks. */
    bool diagnostic = false;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (cfg->checkpoint != PNA_TMS_CHECKPOINT_NONE &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_CONNECTED &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_MANIFEST &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_FIRST_SECTOR &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_FULL_FILE &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_ALL_FILES_MD5 &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_MANAGED_READY &&
        cfg->checkpoint != PNA_TMS_CHECKPOINT_REVERIFY_STAGED)
        return false;
    diagnostic = cfg->checkpoint != PNA_TMS_CHECKPOINT_NONE;
#endif
    bool needsStaging = !diagnostic;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    if (cfg->checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE ||
        cfg->checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE ||
        cfg->checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 ||
        cfg->checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
        cfg->checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY ||
        cfg->checkpoint == PNA_TMS_CHECKPOINT_REVERIFY_STAGED)
        needsStaging = true;
#endif
    if (needsStaging) {
        if (cfg->stagingPrefix[0] == '\0' ||
            !memchr(cfg->stagingPrefix, '\0', sizeof(cfg->stagingPrefix)) ||
            strlen(cfg->stagingPrefix) + sizeof("_00.bin") >
                PNA_TMS_STAGE_PATH_SIZE)
            return false;
    }
    if (!diagnostic &&
        (!cfg->package.verify || !cfg->package.beginInstall ||
         !cfg->package.pollInstall))
        return false;
    memset(p, 0, sizeof(*p));
    p->cfg = *cfg;
    if (!p->cfg.maxSectorAttempts)
        p->cfg.maxSectorAttempts = PNA_ATTEMPTS_DEFAULT;
    return true;
}

TmsBackend pnaTmsBackend(PnaTms* p) {
    static const TmsBackendOps ops = {
        .start = startPna, .process = processPna, .cancel = cancelPna
    };
    return (TmsBackend){.ops = &ops, .context = p, .flags = 0};
}
