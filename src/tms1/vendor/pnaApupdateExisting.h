#ifndef PNA_APUPDATE_EXISTING_H_
#define PNA_APUPDATE_EXISTING_H_

#include <stddef.h>
#include <stdint.h>
#include "pnaApupdate.h"

/* Pure, bounded, read-only classification of the first 4096 bytes at the
 * protected platform-updater destination. Classification is NOT evidence of
 * ownership, pending installation, authenticity, or permission to replace. */
typedef enum {
    PNA_EXISTING_HEADER_UNKNOWN = 0,
    PNA_EXISTING_HEADER_BINARY = 1,
    PNA_EXISTING_HEADER_REWRITTEN = 2,
    PNA_EXISTING_HEADER_TOO_SHORT = 3
} PnaApupdateExistingFormat;

PnaApupdateExistingFormat pnaApupdateExistingClassify(
    const uint8_t* header, size_t headerLength, uint64_t destinationBytes);

const char* pnaApupdateExistingFormatName(PnaApupdateExistingFormat format);

/* Stage 14C: classify a complete 4096-byte header independently of its
 * physical file size. A binary container is accepted only if its embedded
 * payload ranges fit within physicalBytes; any extra tail remains untrusted.
 * No file I/O or allocation occurs here. */
typedef struct {
    PnaApupdateExistingFormat format;
    PnaApupdateResult parseResult;
    uint32_t storedHeaderCrc;
    uint32_t calculatedHeaderCrc;
    bool headerCrcMatches;
    uint8_t claimedFileCount;
    uint64_t logicalBytes; /* meaningful only for BINARY */
    uint64_t trailingBytes; /* meaningful only for BINARY */
} PnaApupdateExistingAnalysis;

bool pnaApupdateExistingAnalyze(const uint8_t* header, size_t headerLen,
                                uint64_t physicalBytes,
                                PnaApupdateInfo* entries,
                                PnaApupdateExistingAnalysis* analysis);

/* Copy an untrusted fixed-size field for safe diagnostic logging. Only a
 * bounded printable ASCII prefix is retained; this is not a name parser. */
void pnaApupdateExistingText(const uint8_t* bytes, size_t length,
                             char* output, size_t outputCapacity);

#endif
