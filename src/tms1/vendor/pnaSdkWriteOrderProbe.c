#include "pnaSdkWriteOrderProbe.h"

#include <string.h>

#define ORDER_PREFIX "/mtd0/tms_15c_"
#define ORDER_EXTENSION ".bin"
#define ORDER_MAX_PATH 31u
#define ORDER_BYTES 16u
#define ORDER_TOTAL (ORDER_BYTES * 2u)

/* Distinct patterns make wrong-offset writes and clobbered tails observable. */
static const uint8_t originalHead[ORDER_BYTES] = {
    0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
    0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xf0,0x01
};
static const uint8_t replacementHead[ORDER_BYTES] = {
    'T','M','S','-','1','5','C','-','H','E','A','D','E','R','!','!'
};
static const uint8_t payload[ORDER_BYTES] = {
    0xa0,0xb1,0xc2,0xd3,0xe4,0xf5,0x06,0x17,
    0x28,0x39,0x4a,0x5b,0x6c,0x7d,0x8e,0x9f
};

bool pnaSdkOrderCanStart(const char* path) {
    if (!path) return false;
    const char* end = (const char*)memchr(path, '\0', ORDER_MAX_PATH + 1u);
    if (!end) return false;
    const size_t length = (size_t)(end - path);
    const size_t prefixLength = sizeof(ORDER_PREFIX) - 1u;
    const size_t extensionLength = sizeof(ORDER_EXTENSION) - 1u;
    if (length > ORDER_MAX_PATH || length <= prefixLength + extensionLength ||
        memcmp(path, ORDER_PREFIX, prefixLength) != 0 ||
        memcmp(path + length - extensionLength, ORDER_EXTENSION,
               extensionLength) != 0)
        return false;
    for (size_t i = prefixLength; i < length - extensionLength; ++i) {
        const char c = path[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

bool pnaSdkOrderBegin(PnaSdkWriteOrderProbe* probe, const char* path) {
    if (!probe || !pnaSdkOrderCanStart(path)) return false;
    memset(probe, 0, sizeof(*probe));
    memcpy(probe->path, path, strlen(path) + 1u);
    probe->phase = PNA_SDK_ORDER_CHECK_ABSENT;
    probe->aSize = -1;
    probe->bSizeBefore = -1;
    probe->bSizeAfter = -1;
    return true;
}

bool pnaSdkOrderAbort(PnaSdkWriteOrderProbe* probe) {
    if (!probe || !pnaSdkOrderCanStart(probe->path)) return false;
    bool closed = true;
    if (probe->handle) {
        FileHandle* handle = probe->handle;
        probe->handle = NULL;
        closed = OOP_CALL(file(), close, handle) == 0;
    }
    if (!probe->owned) return closed;
    const int removed = OOP_CALL(file(), remove, probe->path);
    probe->cleanupOk = removed == 0 &&
                       !OOP_CALL(file(), exists, probe->path);
    if (probe->cleanupOk) probe->owned = false;
    return probe->cleanupOk && closed;
}

static PnaSdkOrderStepResult failProbe(PnaSdkWriteOrderProbe* probe, int error) {
    probe->failedPhase = (int)probe->phase;
    (void)pnaSdkOrderAbort(probe);
    probe->lastHalCode = error;
    probe->phase = PNA_SDK_ORDER_FAILED;
    return PNA_SDK_ORDER_STEP_FAILED;
}

static bool writeBytes(PnaSdkWriteOrderProbe* probe,
                       const uint8_t bytes[ORDER_BYTES], bool overwrite) {
    if (!probe->handle) return false;
    const size_t written = overwrite
        ? OOP_CALL(file(), overwrite, bytes, 1u, ORDER_BYTES, probe->handle)
        : OOP_CALL(file(), write, bytes, 1u, ORDER_BYTES, probe->handle);
    return written == ORDER_BYTES;
}

/* Read exactly 16 bytes at a known offset. A mismatch or short read is a
 * diagnostic result, not an invitation to overwrite an existing file. */
static PnaSdkOrderReadResult compareBytes(PnaSdkWriteOrderProbe* probe,
                                         long offset,
                                         const uint8_t expected[ORDER_BYTES]) {
    uint8_t received[ORDER_BYTES] = {0};
    if (!probe->handle || OOP_CALL(file(), seek, probe->handle,
                                  offset, FILE_SEEK_ORG_SET) != 0)
        return PNA_SDK_ORDER_UNREADABLE;
    const size_t got = OOP_CALL(file(), read, received, 1u,
                                sizeof(received), probe->handle);
    if (!got) return PNA_SDK_ORDER_UNREADABLE;
    if (got != sizeof(received)) return PNA_SDK_ORDER_PARTIAL;
    return memcmp(received, expected, sizeof(received)) == 0
        ? PNA_SDK_ORDER_MATCH : PNA_SDK_ORDER_CHANGED;
}

static bool closeOwnedHandle(PnaSdkWriteOrderProbe* probe) {
    if (!probe->handle) return false;
    FileHandle* handle = probe->handle;
    probe->handle = NULL;
    return OOP_CALL(file(), close, handle) == 0;
}

static bool removeOwnedFile(PnaSdkWriteOrderProbe* probe) {
    return probe->owned && OOP_CALL(file(), remove, probe->path) == 0;
}

PnaSdkOrderStepResult pnaSdkOrderStep(PnaSdkWriteOrderProbe* probe) {
    if (!probe || !pnaSdkOrderCanStart(probe->path))
        return PNA_SDK_ORDER_STEP_FAILED;

    probe->lastHalCode = 0;
    switch (probe->phase) {
    case PNA_SDK_ORDER_CHECK_ABSENT:
        if (OOP_CALL(file(), exists, probe->path)) return failProbe(probe, -1);
        probe->phase = PNA_SDK_ORDER_A_OPEN;
        break;

    case PNA_SDK_ORDER_A_OPEN:
        /* If open creates the file and then fails, cleanup still owns it. */
        probe->owned = true;
        probe->handle = OOP_CALL(file(), open, probe->path, "wb");
        if (!probe->handle) return failProbe(probe, -2);
        probe->phase = PNA_SDK_ORDER_A_HEADER_FIRST;
        break;

    case PNA_SDK_ORDER_A_HEADER_FIRST:
        if (!writeBytes(probe, replacementHead, true))
            return failProbe(probe, -3);
        probe->phase = PNA_SDK_ORDER_A_APPEND_PAYLOAD;
        break;

    case PNA_SDK_ORDER_A_APPEND_PAYLOAD:
        /* HAL overwrite() sets handle position to 16 on success. This write
         * uses sdkFileInsert() indirectly, at contiguous offset 16. */
        if (!writeBytes(probe, payload, false)) return failProbe(probe, -4);
        probe->phase = PNA_SDK_ORDER_A_VERIFY;
        break;

    case PNA_SDK_ORDER_A_VERIFY:
        probe->aSize = OOP_CALL(file(), size, probe->handle);
        probe->aHeader = compareBytes(probe, 0, replacementHead);
        probe->aPayload = compareBytes(probe, (long)ORDER_BYTES, payload);
        probe->viableA = probe->aSize >= (long)ORDER_TOTAL &&
            probe->aHeader == PNA_SDK_ORDER_MATCH &&
            probe->aPayload == PNA_SDK_ORDER_MATCH;
        probe->phase = PNA_SDK_ORDER_A_CLOSE;
        break;

    case PNA_SDK_ORDER_A_CLOSE:
        if (!closeOwnedHandle(probe)) return failProbe(probe, -5);
        probe->phase = PNA_SDK_ORDER_A_DELETE;
        break;

    case PNA_SDK_ORDER_A_DELETE:
        if (!removeOwnedFile(probe)) return failProbe(probe, -6);
        probe->phase = PNA_SDK_ORDER_A_CHECK_DELETED;
        break;

    case PNA_SDK_ORDER_A_CHECK_DELETED:
        if (OOP_CALL(file(), exists, probe->path))
            return failProbe(probe, -7);
        probe->owned = false;
        probe->cleanupOk = true;
        probe->phase = PNA_SDK_ORDER_B_OPEN;
        break;

    case PNA_SDK_ORDER_B_OPEN:
        /* Never open an unexpectedly present file. A was deleted first. */
        if (OOP_CALL(file(), exists, probe->path))
            return failProbe(probe, -8);
        probe->owned = true;
        probe->cleanupOk = false;
        probe->handle = OOP_CALL(file(), open, probe->path, "wb");
        if (!probe->handle) return failProbe(probe, -9);
        probe->phase = PNA_SDK_ORDER_B_WRITE_HEADER;
        break;

    case PNA_SDK_ORDER_B_WRITE_HEADER:
        if (!writeBytes(probe, originalHead, false))
            return failProbe(probe, -10);
        probe->phase = PNA_SDK_ORDER_B_APPEND_PAYLOAD;
        break;

    case PNA_SDK_ORDER_B_APPEND_PAYLOAD:
        if (!writeBytes(probe, payload, false))
            return failProbe(probe, -11);
        probe->phase = PNA_SDK_ORDER_B_VERIFY_INITIAL;
        break;

    case PNA_SDK_ORDER_B_VERIFY_INITIAL:
        probe->bSizeBefore = OOP_CALL(file(), size, probe->handle);
        probe->initialBVerified = probe->bSizeBefore >= (long)ORDER_TOTAL &&
            compareBytes(probe, 0, originalHead) == PNA_SDK_ORDER_MATCH &&
            compareBytes(probe, (long)ORDER_BYTES, payload) == PNA_SDK_ORDER_MATCH;
        if (!probe->initialBVerified) return failProbe(probe, -12);
        probe->phase = PNA_SDK_ORDER_B_SEEK_HEADER;
        break;

    case PNA_SDK_ORDER_B_SEEK_HEADER:
        if (OOP_CALL(file(), seek, probe->handle,
                     0L, FILE_SEEK_ORG_SET) != 0)
            return failProbe(probe, -13);
        probe->phase = PNA_SDK_ORDER_B_REPLACE_HEADER;
        break;

    case PNA_SDK_ORDER_B_REPLACE_HEADER:
        /* HAL write() normalizes raw SDK success, including SDK_FILE_OK=101.
         * Using a seek to offset zero exercises positioned sdkFileInsert(). */
        if (!writeBytes(probe, replacementHead, false))
            return failProbe(probe, -14);
        probe->phase = PNA_SDK_ORDER_B_VERIFY_FINAL;
        break;

    case PNA_SDK_ORDER_B_VERIFY_FINAL:
        probe->bSizeAfter = OOP_CALL(file(), size, probe->handle);
        probe->bHeader = compareBytes(probe, 0, replacementHead);
        probe->bPayload = compareBytes(probe, (long)ORDER_BYTES, payload);
        probe->viableB = probe->bSizeAfter >= (long)ORDER_TOTAL &&
            probe->bHeader == PNA_SDK_ORDER_MATCH &&
            probe->bPayload == PNA_SDK_ORDER_MATCH;
        probe->phase = PNA_SDK_ORDER_B_CLOSE;
        break;

    case PNA_SDK_ORDER_B_CLOSE:
        if (!closeOwnedHandle(probe)) return failProbe(probe, -15);
        probe->phase = PNA_SDK_ORDER_B_DELETE;
        break;

    case PNA_SDK_ORDER_B_DELETE:
        if (!removeOwnedFile(probe)) return failProbe(probe, -16);
        probe->phase = PNA_SDK_ORDER_B_CHECK_DELETED;
        break;

    case PNA_SDK_ORDER_B_CHECK_DELETED:
        if (OOP_CALL(file(), exists, probe->path))
            return failProbe(probe, -17);
        probe->owned = false;
        probe->cleanupOk = true;
        probe->phase = PNA_SDK_ORDER_COMPLETE;
        return PNA_SDK_ORDER_STEP_DONE;

    case PNA_SDK_ORDER_COMPLETE:
        return PNA_SDK_ORDER_STEP_DONE;
    case PNA_SDK_ORDER_FAILED:
    default:
        return PNA_SDK_ORDER_STEP_FAILED;
    }
    return PNA_SDK_ORDER_STEP_BUSY;
}
