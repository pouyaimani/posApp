#include "pnaTmsStage.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "sys/sys.h"
#include "file/file.h"

#define STAGE_HEADER_SIZE 28u
#define STAGE_FILE_SIZE   36u
#define STAGE_CRC_SIZE    4u
#define STAGE_MAX_RECORD                                                       \
    (STAGE_HEADER_SIZE + PNA_TMS_MAX_FILES * STAGE_FILE_SIZE + STAGE_CRC_SIZE)
#define STAGE_IO_CHUNK 1024u

static const uint8_t stageMagic[8] = {'P', 'N', 'A', 'T', 'M', 'S', '8', '!'};

typedef enum {
    SLOT_ABSENT,
    SLOT_VALID,
    SLOT_INVALID,
    SLOT_IO_ERROR
} SlotResult;

static void put16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
}
static void put32(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < 4u; ++i)
        p[i] = (uint8_t)(value >> (8u * i));
}
static void put64(uint8_t* p, uint64_t value) {
    for (unsigned i = 0; i < 8u; ++i)
        p[i] = (uint8_t)(value >> (8u * i));
}
static uint16_t get16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}
static uint32_t get32(const uint8_t* p) {
    uint32_t result = 0u;
    for (unsigned i = 0; i < 4u; ++i)
        result |= (uint32_t)p[i] << (8u * i);
    return result;
}
static uint64_t get64(const uint8_t* p) {
    uint64_t result = 0u;
    for (unsigned i = 0; i < 8u; ++i)
        result |= (uint64_t)p[i] << (8u * i);
    return result;
}
static uint32_t checksum(const uint8_t* bytes, size_t count) {
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1u) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc ^ UINT32_C(0xffffffff);
}
static bool validPrefix(const char* prefix) {
    if (!prefix || !prefix[0])
        return false;
    size_t count = 0u;
    while (count <= 95u && prefix[count])
        ++count;
    return count > 0u && count <= 95u;
}
static bool slotPath(const char* prefix, unsigned slot, char* output,
                     size_t capacity) {
    if (!validPrefix(prefix) || slot > 1u || !output)
        return false;
    int len = snprintf(output, capacity, "%s.j%u", prefix, slot);
    return len > 0 && (size_t)len < capacity;
}
bool pnaTmsStageFilePath(const char* prefix, size_t index, char* output,
                         size_t capacity) {
    if (!validPrefix(prefix) || index >= PNA_TMS_MAX_FILES || !output)
        return false;
    int len =
        snprintf(output, capacity, "%s_%02u.bin", prefix, (unsigned)index);
    return len > 0 && (size_t)len < capacity;
}
static bool validInfo(const PnaTmsStageInfo* info) {
    if (!info || info->fileCount == 0u || info->fileCount > PNA_TMS_MAX_FILES ||
        info->verifiedCount > info->fileCount ||
        (info->status != PNA_TMS_STAGE_INCOMPLETE &&
         info->status != PNA_TMS_STAGE_READY_UNTRUSTED) ||
        (info->status == PNA_TMS_STAGE_READY_UNTRUSTED &&
         info->verifiedCount != info->fileCount))
        return false;
    uint64_t total = 0u;
    for (unsigned i = 0u; i < info->fileCount; ++i) {
        if (info->files[i].size > PNA_TMS_MAX_FILE_SIZE)
            return false;
        total += info->files[i].size;
    }
    return total == info->totalBytes;
}
static size_t recordLength(unsigned fileCount) {
    return STAGE_HEADER_SIZE + (size_t)fileCount * STAGE_FILE_SIZE +
           STAGE_CRC_SIZE;
}
static void encode(const PnaTmsStageInfo* info, uint8_t* raw) {
    const size_t length = recordLength(info->fileCount);
    memset(raw, 0, length);
    memcpy(raw, stageMagic, sizeof(stageMagic));
    raw[8] = 1u;
    raw[9] = info->status == PNA_TMS_STAGE_READY_UNTRUSTED ? 2u : 1u;
    put32(raw + 12u, info->sequence);
    put16(raw + 16u, info->fileCount);
    put16(raw + 18u, info->verifiedCount);
    put64(raw + 20u, info->totalBytes);
    for (unsigned i = 0u; i < info->fileCount; ++i) {
        uint8_t* item = raw + STAGE_HEADER_SIZE + (size_t)i * STAGE_FILE_SIZE;
        memcpy(item, info->files[i].name, PNA_TMS_FILE_NAME_SIZE);
        put32(item + 16u, info->files[i].size);
        memcpy(item + 20u, info->files[i].md5, PNA_TMS_MD5_SIZE);
    }
    put32(raw + length - STAGE_CRC_SIZE,
          checksum(raw, length - STAGE_CRC_SIZE));
}
static bool decode(const uint8_t* raw, size_t length, PnaTmsStageInfo* info) {
    if (length < recordLength(1u) ||
        memcmp(raw, stageMagic, sizeof(stageMagic)) != 0 || raw[8] != 1u ||
        (raw[9] != 1u && raw[9] != 2u) || raw[10] != 0u || raw[11] != 0u)
        return false;
    const uint16_t count = get16(raw + 16u);
    if (count == 0u || count > PNA_TMS_MAX_FILES ||
        length != recordLength(count) ||
        checksum(raw, length - STAGE_CRC_SIZE) !=
            get32(raw + length - STAGE_CRC_SIZE))
        return false;
    memset(info, 0, sizeof(*info));
    info->sequence = get32(raw + 12u);
    info->status =
        raw[9] == 2u ? PNA_TMS_STAGE_READY_UNTRUSTED : PNA_TMS_STAGE_INCOMPLETE;
    info->fileCount     = count;
    info->verifiedCount = get16(raw + 18u);
    info->totalBytes    = get64(raw + 20u);
    for (unsigned i = 0u; i < count; ++i) {
        const uint8_t* item =
            raw + STAGE_HEADER_SIZE + (size_t)i * STAGE_FILE_SIZE;
        memcpy(info->files[i].name, item, PNA_TMS_FILE_NAME_SIZE);
        info->files[i].size = get32(item + 16u);
        memcpy(info->files[i].md5, item + 20u, PNA_TMS_MD5_SIZE);
    }
    return validInfo(info);
}
static SlotResult readSlot(const char* prefix, unsigned slot, uint8_t* scratch,
                           PnaTmsStageInfo* out) {
    char path[128];
    if (!slotPath(prefix, slot, path, sizeof(path)))
        return SLOT_IO_ERROR;
    if (!OOP_CALL(file(), exists, path))
        return SLOT_ABSENT;
    FileHandle* f = OOP_CALL(file(), open, path, "rb");
    if (!f)
        return SLOT_IO_ERROR;
    bool ok       = true;
    long fileSize = OOP_CALL(file(), size, f);
    if (fileSize < (long)recordLength(1u) || fileSize > (long)STAGE_MAX_RECORD)
        ok = false;
    if (ok) {
        size_t offset = 0u;
        while (offset < (size_t)fileSize) {
            size_t remaining = (size_t)fileSize - offset;
            if (remaining > STAGE_IO_CHUNK)
                remaining = STAGE_IO_CHUNK;
            if (OOP_CALL(file(), read, scratch + offset, 1u, remaining, f) !=
                remaining) {
                ok = false;
                break;
            }
            offset += remaining;
        }
    }
    if (OOP_CALL(file(), close, f) != 0)
        return SLOT_IO_ERROR;
    if (!ok)
        return SLOT_INVALID;
    return decode(scratch, (size_t)fileSize, out) ? SLOT_VALID : SLOT_INVALID;
}
/* A READY marker is never sufficient: at least require all staged paths
 * to exist with the expected lengths. This is not a substitute for an MD5
 * recheck, and neither check authenticates the software publisher. */
static PnaTmsStageStatus checkReadyFiles(const char*            prefix,
                                         const PnaTmsStageInfo* info) {
    for (size_t i = 0u; i < info->fileCount; ++i) {
        char path[128];
        if (!pnaTmsStageFilePath(prefix, i, path, sizeof(path)))
            return PNA_TMS_STAGE_IO_ERROR;
        if (!OOP_CALL(file(), exists, path))
            return PNA_TMS_STAGE_INCOMPLETE;
        FileHandle* f = OOP_CALL(file(), open, path, "rb");
        if (!f)
            return PNA_TMS_STAGE_IO_ERROR;
        long actual      = OOP_CALL(file(), size, f);
        int  closeResult = OOP_CALL(file(), close, f);
        if (actual < 0 || closeResult != 0)
            return PNA_TMS_STAGE_IO_ERROR;
        if ((uint64_t)actual != info->files[i].size)
            return PNA_TMS_STAGE_INCOMPLETE;
    }
    return PNA_TMS_STAGE_READY_UNTRUSTED;
}

PnaTmsStageStatus pnaTmsStageInspect(const char*      prefix,
                                     PnaTmsStageInfo* info) {
    if (!validPrefix(prefix))
        return PNA_TMS_STAGE_IO_ERROR;
    uint8_t*         scratch = (uint8_t*)MEM_ALLOC(STAGE_MAX_RECORD);
    PnaTmsStageInfo* candidate =
        (PnaTmsStageInfo*)MEM_ALLOC(sizeof(*candidate));
    PnaTmsStageInfo* current = (PnaTmsStageInfo*)MEM_ALLOC(sizeof(*current));
    if (!scratch || !candidate || !current) {
        MEM_FREE(scratch);
        MEM_FREE(candidate);
        MEM_FREE(current);
        return PNA_TMS_STAGE_IO_ERROR;
    }
    bool anyPresent = false, haveValid = false, anyIO = false;
    for (unsigned slot = 0u; slot < 2u; ++slot) {
        SlotResult result = readSlot(prefix, slot, scratch, candidate);
        if (result == SLOT_IO_ERROR)
            anyIO = true;
        if (result != SLOT_ABSENT)
            anyPresent = true;
        if (result == SLOT_VALID &&
            (!haveValid ||
             (int32_t)(candidate->sequence - current->sequence) > 0)) {
            *current  = *candidate;
            haveValid = true;
        }
    }
    PnaTmsStageStatus status =
        anyIO ? PNA_TMS_STAGE_IO_ERROR
              : (haveValid ? current->status
                           : (anyPresent ? PNA_TMS_STAGE_CORRUPT
                                         : PNA_TMS_STAGE_EMPTY));
    if (!anyIO && haveValid && status == PNA_TMS_STAGE_READY_UNTRUSTED)
        status = checkReadyFiles(prefix, current);
    if (haveValid && !anyIO && info) {
        *info        = *current;
        info->status = status;
    }
    MEM_FREE(current);
    MEM_FREE(candidate);
    MEM_FREE(scratch);
    return status;
}
static bool commit(const char* prefix, PnaTmsStageInfo* info,
                   PnaTmsStageStatus status, uint16_t verifiedCount) {
    if (!validPrefix(prefix) || !info)
        return false;
    /* Keep the entire manifest off the device task's small stack. */
    PnaTmsStageInfo* next = (PnaTmsStageInfo*)MEM_ALLOC(sizeof(*next));
    uint8_t*         raw  = (uint8_t*)MEM_ALLOC(STAGE_MAX_RECORD);
    if (!next || !raw) {
        MEM_FREE(next);
        MEM_FREE(raw);
        return false;
    }
    *next = *info;
    next->sequence++;
    next->status        = status;
    next->verifiedCount = verifiedCount;
    if (!validInfo(next)) {
        MEM_FREE(next);
        MEM_FREE(raw);
        return false;
    }
    encode(next, raw);
    char path[128];
    bool ok = slotPath(prefix, (next->sequence - 1u) & 1u, path, sizeof(path));
    FileHandle* f = ok ? OOP_CALL(file(), open, path, "wb") : NULL;
    if (!f)
        ok = false;
    if (f) {
        size_t length = recordLength(next->fileCount);
        size_t offset = 0u;
        while (ok && offset < length) {
            size_t chunk = length - offset;
            if (chunk > STAGE_IO_CHUNK)
                chunk = STAGE_IO_CHUNK;
            if (OOP_CALL(file(), write, raw + offset, 1u, chunk, f) != chunk)
                ok = false;
            offset += chunk;
        }
        if (OOP_CALL(file(), flush, f) != 0)
            ok = false;
        if (OOP_CALL(file(), sync, f) != 0)
            ok = false;
        if (OOP_CALL(file(), close, f) != 0)
            ok = false;
    }
    if (ok)
        *info = *next;
    MEM_FREE(next);
    MEM_FREE(raw);
    return ok;
}

bool pnaTmsStageBegin(const char* prefix, const PnaTmsFile* files, size_t count,
                      uint64_t totalBytes, PnaTmsStageInfo* inOut) {
    if (!inOut || !files || count == 0u || count > PNA_TMS_MAX_FILES ||
        pnaTmsStageInspect(prefix, NULL) != PNA_TMS_STAGE_EMPTY)
        return false;
    memset(inOut, 0, sizeof(*inOut));
    inOut->fileCount  = (uint16_t)count;
    inOut->totalBytes = totalBytes;
    inOut->status     = PNA_TMS_STAGE_INCOMPLETE;
    memcpy(inOut->files, files, count * sizeof(*files));
    if (!validInfo(inOut))
        return false;
    if (commit(prefix, inOut, PNA_TMS_STAGE_INCOMPLETE, 0u))
        return true;
    /* An initial torn record is not ownership metadata: no staged data exists.
     */
    char path[128];
    if (slotPath(prefix, 0u, path, sizeof(path)) &&
        OOP_CALL(file(), exists, path))
        (void)OOP_CALL(file(), remove, path);
    return false;
}
bool pnaTmsStageMarkVerified(const char* prefix, PnaTmsStageInfo* inOut,
                             size_t verifiedCount) {
    if (!inOut || inOut->status != PNA_TMS_STAGE_INCOMPLETE ||
        verifiedCount != (size_t)inOut->verifiedCount + 1u ||
        verifiedCount > inOut->fileCount)
        return false;
    return commit(prefix, inOut, PNA_TMS_STAGE_INCOMPLETE,
                  (uint16_t)verifiedCount);
}
bool pnaTmsStageMarkReady(const char* prefix, PnaTmsStageInfo* inOut) {
    if (!inOut || inOut->status != PNA_TMS_STAGE_INCOMPLETE ||
        inOut->verifiedCount != inOut->fileCount)
        return false;
    return commit(prefix, inOut, PNA_TMS_STAGE_READY_UNTRUSTED,
                  inOut->verifiedCount);
}
bool pnaTmsStageDiscard(const char* prefix, bool discardReady) {
    PnaTmsStageInfo* info = (PnaTmsStageInfo*)MEM_ALLOC(sizeof(*info));
    if (!info)
        return false;
    PnaTmsStageStatus status = pnaTmsStageInspect(prefix, info);
    if (status == PNA_TMS_STAGE_EMPTY) {
        MEM_FREE(info);
        return true;
    }
    if (status != PNA_TMS_STAGE_INCOMPLETE &&
        !(discardReady && status == PNA_TMS_STAGE_READY_UNTRUSTED)) {
        MEM_FREE(info);
        return false;
    }
    bool ok = true;
    for (size_t i = 0u; i < info->fileCount; ++i) {
        char path[128];
        if (!pnaTmsStageFilePath(prefix, i, path, sizeof(path))) {
            ok = false;
            break;
        }
        if (OOP_CALL(file(), exists, path) &&
            OOP_CALL(file(), remove, path) != 0)
            ok = false;
    }
    if (ok) {
        for (unsigned slot = 0u; slot < 2u; ++slot) {
            char path[128];
            if (!slotPath(prefix, slot, path, sizeof(path))) {
                ok = false;
                break;
            }
            if (OOP_CALL(file(), exists, path) &&
                OOP_CALL(file(), remove, path) != 0)
                ok = false;
        }
    }
    MEM_FREE(info);
    return ok;
}
