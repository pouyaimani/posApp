#ifndef PNA_APUPDATE_H_
#define PNA_APUPDATE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Legacy platform-specific apupdate container. The compiler's packed
 * struct layout is represented explicitly to avoid unaligned u32 reads. */
#define PNA_APUPDATE_HEADER_LEN 4096u
#define PNA_APUPDATE_HEAD_INFO_LEN 160u
#define PNA_APUPDATE_FILE_INFO_LEN 204u
#define PNA_APUPDATE_MAX_FILES 15u
#define PNA_APUPDATE_NAME_LEN 20u

/* A read-only description. A raw CRC is the uncomplemented legacy value. */
typedef struct {
    char name[PNA_APUPDATE_NAME_LEN + 1u];
    uint32_t offset;
    uint32_t size;
    uint32_t rawCrc;
} PnaApupdateEntry;

typedef struct {
    uint8_t fileCount;
    uint32_t headerCrc;
    uint64_t packageSize;
    PnaApupdateEntry entries[PNA_APUPDATE_MAX_FILES];
} PnaApupdateInfo;

typedef enum {
    PNA_APUPDATE_OK = 0,
    PNA_APUPDATE_BAD_INPUT,
    PNA_APUPDATE_HEADER_CRC,
    PNA_APUPDATE_FILE_COUNT,
    PNA_APUPDATE_INVALID_NAME,
    PNA_APUPDATE_BOUNDS,
    PNA_APUPDATE_SIZE_MISMATCH,
    PNA_APUPDATE_BUFFER_SMALL
} PnaApupdateResult;

/* Legacy rolling CRC32: initialize to 0xFFFFFFFF and DO NOT XOR final CRC
 * for header/embedded-file comparison. The text install instructions use ~CRC. */
uint32_t pnaApupdateCrc32(uint32_t crc, const uint8_t* data, size_t len);

/* Requires the entire 4096-byte header. Does no file I/O or allocation.
 * `packageSize` is the actual File HAL size. All declared payloads must fit.
 * Exact total size is required: no hidden extra bytes accepted. */
PnaApupdateResult pnaApupdateParse(const uint8_t* header, size_t headerLen,
                                   uint64_t packageSize, PnaApupdateInfo* out);

/* Builds the legacy updater's INI header ONLY IN MEMORY after every payload
 * CRC has been confirmed by the caller. Does not set a flag or write a file.
 * The output is a 4096-byte NUL-padded image, ready to inspect, NOT install. */
PnaApupdateResult pnaApupdateBuildPreview(const PnaApupdateInfo* info,
                                          uint8_t* out, size_t capacity,
                                          size_t* textLen);
#endif
