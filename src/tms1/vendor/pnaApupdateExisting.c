#include "pnaApupdateExisting.h"
#include "pnaApupdate.h"

#include <stdbool.h>
#include <string.h>

/* Search only inside the bounded NUL-terminated INI portion of the reserved
 * header; do not treat binary data as a C string or log untrusted contents. */
static bool findSection(const uint8_t* buffer, size_t length,
                        const char* section, size_t* position) {
    size_t needle = strlen(section);
    if (!buffer || !position || needle > length)
        return false;
    for (size_t i = 0u; i <= length - needle; ++i) {
        if (memcmp(buffer + i, section, needle) == 0) {
            *position = i;
            return true;
        }
    }
    return false;
}

static bool looksRewritten(const uint8_t* header) {
    static const char* const sections[] = {"[namelist]\r\n", "[adrlist]\r\n",
                                           "[lenlist]\r\n", "[crclist]\r\n"};
    size_t                   length     = 0u;
    while (length < PNA_APUPDATE_HEADER_LEN && header[length] != 0u)
        ++length;
    if (length == 0u || length == PNA_APUPDATE_HEADER_LEN ||
        length < strlen(sections[0]) ||
        memcmp(header, sections[0], strlen(sections[0])) != 0)
        return false;
    /* The legacy rewritten header occupies a NUL-padded 4096-byte region. */
    for (size_t i = length; i < PNA_APUPDATE_HEADER_LEN; ++i)
        if (header[i] != 0u)
            return false;
    size_t previous = 0u;
    for (size_t i = 1u; i < sizeof(sections) / sizeof(sections[0]); ++i) {
        size_t pos = 0u;
        if (!findSection(header + previous + 1u, length - (previous + 1u),
                         sections[i], &pos))
            return false;
        previous += 1u + pos;
    }
    return true;
}

PnaApupdateExistingFormat
pnaApupdateExistingClassify(const uint8_t* header, size_t headerLength,
                            uint64_t destinationBytes) {
    if (!header || headerLength < PNA_APUPDATE_HEADER_LEN ||
        destinationBytes < PNA_APUPDATE_HEADER_LEN)
        return PNA_EXISTING_HEADER_TOO_SHORT;
    PnaApupdateInfo parsed;
    if (pnaApupdateParse(header, PNA_APUPDATE_HEADER_LEN, destinationBytes,
                         &parsed) == PNA_APUPDATE_OK)
        return PNA_EXISTING_HEADER_BINARY;
    if (looksRewritten(header))
        return PNA_EXISTING_HEADER_REWRITTEN;
    return PNA_EXISTING_HEADER_UNKNOWN;
}

const char* pnaApupdateExistingFormatName(PnaApupdateExistingFormat format) {
    switch (format) {
    case PNA_EXISTING_HEADER_BINARY:
        return "BINARY";
    case PNA_EXISTING_HEADER_REWRITTEN:
        return "REWRITTEN_INI";
    case PNA_EXISTING_HEADER_TOO_SHORT:
        return "TOO_SHORT";
    default:
        return "UNKNOWN";
    }
}

static uint32_t load32(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

bool pnaApupdateExistingAnalyze(const uint8_t* header, size_t headerLen,
                                uint64_t                     physicalBytes,
                                PnaApupdateInfo*             entries,
                                PnaApupdateExistingAnalysis* analysis) {
    if (!entries || !analysis)
        return false;
    memset(entries, 0, sizeof(*entries));
    memset(analysis, 0, sizeof(*analysis));
    analysis->format      = PNA_EXISTING_HEADER_TOO_SHORT;
    analysis->parseResult = PNA_APUPDATE_BAD_INPUT;
    if (!header || headerLen < PNA_APUPDATE_HEADER_LEN ||
        physicalBytes < PNA_APUPDATE_HEADER_LEN)
        return true;

    analysis->storedHeaderCrc = load32(header);
    analysis->calculatedHeaderCrc =
        pnaApupdateCrc32(UINT32_MAX, header + 4u, PNA_APUPDATE_HEADER_LEN - 4u);
    analysis->headerCrcMatches =
        analysis->storedHeaderCrc == analysis->calculatedHeaderCrc;
    analysis->claimedFileCount = header[4];
    analysis->parseResult      = pnaApupdateParsePadded(
        header, PNA_APUPDATE_HEADER_LEN, physicalBytes, entries);
    if (analysis->parseResult == PNA_APUPDATE_OK) {
        analysis->format        = PNA_EXISTING_HEADER_BINARY;
        analysis->logicalBytes  = entries->packageSize;
        analysis->trailingBytes = physicalBytes - entries->packageSize;
    } else {
        analysis->format = looksRewritten(header)
                               ? PNA_EXISTING_HEADER_REWRITTEN
                               : PNA_EXISTING_HEADER_UNKNOWN;
        memset(entries, 0, sizeof(*entries));
    }
    return true;
}

void pnaApupdateExistingText(const uint8_t* bytes, size_t length, char* output,
                             size_t outputCapacity) {
    if (!output || !outputCapacity)
        return;
    output[0] = '\0';
    if (!bytes)
        return;
    size_t n = 0u;
    while (n + 1u < outputCapacity && n < length && bytes[n]) {
        const unsigned char c = bytes[n];
        output[n]             = (c >= 0x20u && c <= 0x7eu) ? (char)c : '?';
        ++n;
    }
    while (n && output[n - 1u] == ' ')
        --n;
    output[n] = '\0';
}
