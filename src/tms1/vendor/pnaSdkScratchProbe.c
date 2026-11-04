#include "pnaSdkScratchProbe.h"

#include <string.h>

#define SCRATCH_PREFIX "/mtd0/tms_15b_"
/* Conservative probe limit, not a claim about the SDK maximum path size. */
#define SCRATCH_MAX_PATH_LENGTH 31u
#define SCRATCH_EXTENSION ".bin"
#define SCRATCH_PATTERN_BYTES 16u
#define SCRATCH_SECOND_OFFSET SCRATCH_PATTERN_BYTES

/* Short, contiguous test writes via the existing File HAL. */
static const uint8_t pattern0[SCRATCH_PATTERN_BYTES] = {
    0x31,0x42,0x53,0x64,0x75,0x86,0x97,0xa8,
    0xb9,0xca,0xdb,0xec,0xfd,0x0e,0x1f,0x20
};
static const uint8_t patternTail[SCRATCH_PATTERN_BYTES] = {
    0x20,0x1f,0x0e,0xfd,0xec,0xdb,0xca,0xb9,
    0xa8,0x97,0x86,0x75,0x64,0x53,0x42,0x31
};
static const uint8_t rewrittenHead[SCRATCH_PATTERN_BYTES] = {
    'T','M','S','-','F','I','L','E','-','P','R','O','B','E','1','5'
};

static bool validPath(const char* path) {
    if (!path) return false;
    const char* end = (const char*)memchr(path, '\0', SCRATCH_MAX_PATH_LENGTH + 1u);
    if (!end) return false;
    const size_t length = (size_t)(end - path);
    const size_t prefixLen = sizeof(SCRATCH_PREFIX) - 1u;
    const size_t extLen = sizeof(SCRATCH_EXTENSION) - 1u;
    if (length > SCRATCH_MAX_PATH_LENGTH ||
        length <= prefixLen + extLen ||
        memcmp(path, SCRATCH_PREFIX, prefixLen) != 0 ||
        memcmp(path + length - extLen, SCRATCH_EXTENSION, extLen) != 0)
        return false;
    for (size_t i = prefixLen; i < length - extLen; ++i) {
        const char c = path[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

bool pnaSdkScratchCanStart(const char* path) {
    return validPath(path);
}

bool pnaSdkScratchBegin(PnaSdkScratchProbe* probe, const char* path) {
    if (!probe || !validPath(path)) return false;
    memset(probe, 0, sizeof(*probe));
    memcpy(probe->path, path, strlen(path) + 1u);
    probe->phase = PNA_SDK_SCRATCH_CHECK_ABSENT;
    return true;
}

bool pnaSdkScratchAbort(PnaSdkScratchProbe* probe) {
    if (!probe || !validPath(probe->path)) return false;
    bool closed = true;
    if (probe->handle) {
        FileHandle* handle = probe->handle;
        probe->handle = NULL;
        closed = OOP_CALL(file(), close, handle) == 0;
    }
    if (!probe->owned) return closed;
    const int ret = OOP_CALL(file(), remove, probe->path);
    probe->lastHalCode = ret;
    probe->cleanupOk = ret == 0 && !OOP_CALL(file(), exists, probe->path);
    if (probe->cleanupOk) probe->owned = false;
    return probe->cleanupOk && closed;
}

static PnaSdkScratchStepResult failed(PnaSdkScratchProbe* probe,
                                     int halCode) {
    probe->failedPhase = (int)probe->phase;
    (void)pnaSdkScratchAbort(probe);
    probe->lastHalCode = halCode; /* Preserve failure cause, not cleanup code. */
    probe->phase = PNA_SDK_SCRATCH_FAILED;
    return PNA_SDK_SCRATCH_STEP_FAILED;
}

static bool readEqual(PnaSdkScratchProbe* probe, long offset,
                      const uint8_t expected[SCRATCH_PATTERN_BYTES]) {
    uint8_t received[SCRATCH_PATTERN_BYTES] = {0};
    if (!probe->handle || OOP_CALL(file(), seek, probe->handle, offset,
                                  FILE_SEEK_ORG_SET) != 0) {
        probe->lastHalCode = -1;
        return false;
    }
    const size_t n = OOP_CALL(file(), read, received, 1u,
                              sizeof(received), probe->handle);
    if (n != sizeof(received)) {
        probe->lastHalCode = -2;
        return false;
    }
    if (memcmp(received, expected, sizeof(received)) != 0) {
        probe->lastHalCode = -3;
        return false;
    }
    probe->lastHalCode = 0;
    return true;
}

PnaSdkScratchStepResult pnaSdkScratchStep(PnaSdkScratchProbe* probe) {
    if (!probe || !validPath(probe->path))
        return PNA_SDK_SCRATCH_STEP_FAILED;

    switch (probe->phase) {
    case PNA_SDK_SCRATCH_CHECK_ABSENT:
        if (OOP_CALL(file(), exists, probe->path)) return failed(probe, -1);
        probe->lastHalCode = 0;
        probe->phase = PNA_SDK_SCRATCH_OPEN_NEW;
        return PNA_SDK_SCRATCH_STEP_BUSY;

    case PNA_SDK_SCRATCH_OPEN_NEW:
        /* May create a file before returning failure; own cleanup either way.
         * HAL open("wb") is NOT assumed to truncate an existing file. */
        probe->owned = true;
        probe->handle = OOP_CALL(file(), open, probe->path, "wb");
        if (!probe->handle) return failed(probe, -2);
        probe->lastHalCode = 0;
        probe->phase = PNA_SDK_SCRATCH_INSERT_ZERO;
        return PNA_SDK_SCRATCH_STEP_BUSY;

    case PNA_SDK_SCRATCH_INSERT_ZERO: {
        /* The HAL write() already interprets sdkFileInsert() against
         * SDK_FILE_OK and returns a byte count. Never assume OK == 0. */
        if (!probe->handle) return failed(probe, -10);
        const size_t written = OOP_CALL(file(), write, pattern0, 1u,
                                        SCRATCH_PATTERN_BYTES, probe->handle);
        probe->lastHalCode = written == SCRATCH_PATTERN_BYTES ? 0 : -11;
        if (written != SCRATCH_PATTERN_BYTES)
            return failed(probe, probe->lastHalCode);
        probe->phase = PNA_SDK_SCRATCH_INSERT_OFFSET;
        return PNA_SDK_SCRATCH_STEP_BUSY;
    }

    case PNA_SDK_SCRATCH_INSERT_OFFSET: {
        /* HAL write advances handle->pos, so the second 16-byte write is
         * contiguous: offset 16, not a sparse offset 64. */
        if (!probe->handle) return failed(probe, -12);
        const size_t written = OOP_CALL(file(), write, patternTail, 1u,
                                        SCRATCH_PATTERN_BYTES, probe->handle);
        probe->lastHalCode = written == SCRATCH_PATTERN_BYTES ? 0 : -13;
        if (written != SCRATCH_PATTERN_BYTES)
            return failed(probe, probe->lastHalCode);
        probe->phase = PNA_SDK_SCRATCH_READ_ZERO;
        return PNA_SDK_SCRATCH_STEP_BUSY;
    }

    case PNA_SDK_SCRATCH_READ_ZERO:
        if (!readEqual(probe, 0, pattern0))
            return failed(probe, probe->lastHalCode);
        probe->phase = PNA_SDK_SCRATCH_READ_OFFSET;
        return PNA_SDK_SCRATCH_STEP_BUSY;

    case PNA_SDK_SCRATCH_READ_OFFSET:
        if (!readEqual(probe, (long)SCRATCH_SECOND_OFFSET, patternTail))
            return failed(probe, probe->lastHalCode);
        probe->phase = PNA_SDK_SCRATCH_REWRITE_HEAD;
        return PNA_SDK_SCRATCH_STEP_BUSY;

    case PNA_SDK_SCRATCH_REWRITE_HEAD: {
        if (!probe->handle) return failed(probe, -3);
        const size_t written = OOP_CALL(file(), overwrite, rewrittenHead, 1u,
                                        SCRATCH_PATTERN_BYTES, probe->handle);
        probe->lastHalCode = written == SCRATCH_PATTERN_BYTES ? 0 : -4;
        if (written != SCRATCH_PATTERN_BYTES)
            return failed(probe, probe->lastHalCode);
        probe->phase = PNA_SDK_SCRATCH_READ_HEAD;
        return PNA_SDK_SCRATCH_STEP_BUSY;
    }

    case PNA_SDK_SCRATCH_READ_HEAD:
        if (!readEqual(probe, 0, rewrittenHead))
            return failed(probe, probe->lastHalCode);
        probe->phase = PNA_SDK_SCRATCH_READ_TAIL;
        return PNA_SDK_SCRATCH_STEP_BUSY;

    case PNA_SDK_SCRATCH_READ_TAIL: {
        uint8_t received[SCRATCH_PATTERN_BYTES] = {0};
        if (!probe->handle || OOP_CALL(file(), seek, probe->handle,
                                      (long)SCRATCH_SECOND_OFFSET,
                                      FILE_SEEK_ORG_SET) != 0) {
            probe->tail = PNA_SDK_SCRATCH_TAIL_UNREADABLE;
        } else {
            const size_t got = OOP_CALL(file(), read, received, 1u,
                                        sizeof(received), probe->handle);
            if (got == 0u)
                probe->tail = PNA_SDK_SCRATCH_TAIL_UNREADABLE;
            else if (got < sizeof(received))
                probe->tail = PNA_SDK_SCRATCH_TAIL_PARTIAL;
            else if (memcmp(received, patternTail, sizeof(received)) == 0)
                probe->tail = PNA_SDK_SCRATCH_TAIL_PRESERVED;
            else
                probe->tail = PNA_SDK_SCRATCH_TAIL_MODIFIED;
        }
        probe->lastHalCode = 0;
        probe->phase = PNA_SDK_SCRATCH_CLOSE;
        return PNA_SDK_SCRATCH_STEP_BUSY;
    }

    case PNA_SDK_SCRATCH_CLOSE: {
        if (!probe->handle) return failed(probe, -5);
        FileHandle* handle = probe->handle;
        probe->handle = NULL;
        const int ret = OOP_CALL(file(), close, handle);
        probe->lastHalCode = ret;
        if (ret != 0) return failed(probe, ret);
        probe->phase = PNA_SDK_SCRATCH_REMOVE;
        return PNA_SDK_SCRATCH_STEP_BUSY;
    }

    case PNA_SDK_SCRATCH_REMOVE: {
        const int ret = OOP_CALL(file(), remove, probe->path);
        probe->lastHalCode = ret;
        if (ret != 0) return failed(probe, ret);
        probe->phase = PNA_SDK_SCRATCH_CHECK_REMOVED;
        return PNA_SDK_SCRATCH_STEP_BUSY;
    }

    case PNA_SDK_SCRATCH_CHECK_REMOVED:
        if (OOP_CALL(file(), exists, probe->path))
            return failed(probe, -6);
        probe->owned = false;
        probe->cleanupOk = true;
        probe->lastHalCode = 0;
        probe->phase = PNA_SDK_SCRATCH_COMPLETE;
        return PNA_SDK_SCRATCH_STEP_PASSED;

    case PNA_SDK_SCRATCH_COMPLETE:
        return PNA_SDK_SCRATCH_STEP_PASSED;
    case PNA_SDK_SCRATCH_FAILED:
    default:
        return PNA_SDK_SCRATCH_STEP_FAILED;
    }
}
