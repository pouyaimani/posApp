#include "pnaApupdate.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Legacy packed FileInfo offsets, assuming u32 = 4 bytes, bool = 1 byte.
 * The fields after `name` are legacy runtime bookkeeping, not used here. */
#define INFO_SIZE_OFF 7u
#define INFO_CRC_OFF 11u
#define INFO_NAME_OFF 151u

static uint32_t loadLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

uint32_t pnaApupdateCrc32(uint32_t crc, const uint8_t* data, size_t len) {
    if (!data && len) return crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1u) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return crc;
}

static bool parseSafeName(char out[PNA_APUPDATE_NAME_LEN + 1u],
                          const uint8_t* input) {
    size_t n = 0;
    while (n < PNA_APUPDATE_NAME_LEN && input[n]) {
        unsigned c = input[n];
        /* Do not permit paths, INI separators or control characters. */
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') )
            return false;
        ++n;
    }
    if (!n) return false;
    if ((n == 1 && input[0] == '.') ||
        (n == 2 && input[0] == '.' && input[1] == '.')) return false;
    /* A NUL-terminated name cannot contain arbitrary extra bytes. */
    for (size_t j = n; j < PNA_APUPDATE_NAME_LEN; ++j)
        if (input[j] != 0u && input[j] != ' ') return false;
    memcpy(out, input, n);
    out[n] = '\0';
    return true;
}

PnaApupdateResult pnaApupdateParse(const uint8_t* header, size_t headerLen,
                                   uint64_t packageSize, PnaApupdateInfo* out) {
    if (!header || !out || headerLen != PNA_APUPDATE_HEADER_LEN ||
        packageSize < PNA_APUPDATE_HEADER_LEN) return PNA_APUPDATE_BAD_INPUT;
    memset(out, 0, sizeof(*out));
    const uint32_t expected = loadLe32(header);
    const uint32_t actual = pnaApupdateCrc32(UINT32_MAX, header + 4u,
                                              PNA_APUPDATE_HEADER_LEN - 4u);
    if (expected != actual) return PNA_APUPDATE_HEADER_CRC;
    const unsigned count = header[4];
    if (count == 0u || count > PNA_APUPDATE_MAX_FILES ||
        PNA_APUPDATE_HEAD_INFO_LEN + count * PNA_APUPDATE_FILE_INFO_LEN >
            PNA_APUPDATE_HEADER_LEN) return PNA_APUPDATE_FILE_COUNT;
    uint64_t offset = PNA_APUPDATE_HEADER_LEN;
    for (unsigned i = 0u; i < count; ++i) {
        const uint8_t* raw = header + PNA_APUPDATE_HEAD_INFO_LEN +
                             i * PNA_APUPDATE_FILE_INFO_LEN;
        PnaApupdateEntry* e = &out->entries[i];
        if (!parseSafeName(e->name, raw + INFO_NAME_OFF))
            return PNA_APUPDATE_INVALID_NAME;
        e->size = loadLe32(raw + INFO_SIZE_OFF);
        e->rawCrc = loadLe32(raw + INFO_CRC_OFF);
        if (!e->size || offset > UINT32_MAX || offset > packageSize ||
            (uint64_t)e->size > packageSize - offset ||
            offset + e->size > UINT32_MAX) return PNA_APUPDATE_BOUNDS;
        e->offset = (uint32_t)offset;
        offset += e->size;
        for (unsigned j = 0u; j < i; ++j)
            if (strcmp(e->name, out->entries[j].name) == 0)
                return PNA_APUPDATE_INVALID_NAME;
    }
    if (offset != packageSize) return PNA_APUPDATE_SIZE_MISMATCH;
    out->fileCount = (uint8_t)count;
    out->headerCrc = actual;
    out->packageSize = packageSize;
    return PNA_APUPDATE_OK;
}

static bool append(char* dst, size_t cap, size_t* used,
                   const char* format, unsigned i,
                   const PnaApupdateEntry* e, unsigned field) {
    int n;
    if (*used >= cap) return false;
    if (field == 0u) n = snprintf(dst + *used, cap - *used, format, i, e->name);
    else if (field == 1u) n = snprintf(dst + *used, cap - *used, format, i, (unsigned)e->offset);
    else if (field == 2u) n = snprintf(dst + *used, cap - *used, format, i, (unsigned)e->size);
    else n = snprintf(dst + *used, cap - *used, format, i, (unsigned)(e->rawCrc ^ UINT32_MAX));
    if (n < 0 || (size_t)n >= cap - *used) return false;
    *used += (size_t)n;
    return true;
}

PnaApupdateResult pnaApupdateBuildPreview(const PnaApupdateInfo* info,
                                          uint8_t* out, size_t capacity,
                                          size_t* textLen) {
    if (textLen) *textLen = 0u;
    if (!info || !out || capacity < PNA_APUPDATE_HEADER_LEN || !textLen ||
        info->fileCount == 0u || info->fileCount > PNA_APUPDATE_MAX_FILES)
        return PNA_APUPDATE_BAD_INPUT;
    memset(out, 0, PNA_APUPDATE_HEADER_LEN);
    char* dst = (char*)out;
    size_t used = 0u;
    static const char* section[4] = {"[namelist]\r\n", "[adrlist]\r\n",
                                      "[lenlist]\r\n", "[crclist]\r\n"};
    static const char* itemFmt[4] = {"file%u=%s\r\n", "adr%u=0x%08X\r\n",
                                     "len%u=%u\r\n", "crc%u=0x%08X\r\n"};
    for (unsigned s = 0u; s < 4u; ++s) {
        size_t n = strlen(section[s]);
        if (n >= PNA_APUPDATE_HEADER_LEN - used) return PNA_APUPDATE_BUFFER_SMALL;
        memcpy(dst + used, section[s], n);
        used += n;
        dst[used] = '\0';
        for (unsigned i = 0; i < info->fileCount; ++i) {
            if (!append(dst, PNA_APUPDATE_HEADER_LEN, &used, itemFmt[s], i,
                        &info->entries[i], s)) return PNA_APUPDATE_BUFFER_SMALL;
        }
    }
    *textLen = used;
    return PNA_APUPDATE_OK;
}
