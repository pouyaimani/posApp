#include "pnaApupdatePlan.h"

#include <string.h>

bool pnaApupdatePlanBuild(const PnaApupdateInfo* info,
                          const char* locallyGeneratedSourcePath,
                          const uint8_t* preparedHeader4096,
                          size_t previewTextBytes,
                          bool sourceReverified,
                          bool embeddedCrcsVerified,
                          PnaApupdatePreparePlan* out) {
    if (!info || !locallyGeneratedSourcePath || !preparedHeader4096 ||
        !out || !sourceReverified || !embeddedCrcsVerified ||
        info->fileCount == 0u || info->fileCount > PNA_APUPDATE_MAX_FILES ||
        info->packageSize <= PNA_APUPDATE_HEADER_LEN ||
        info->packageSize > UINT32_MAX ||
        previewTextBytes == 0u || previewTextBytes >= PNA_APUPDATE_HEADER_LEN)
        return false;
    /* Bounded strlen: an unterminated source path is never dereferenced
     * beyond the maximum representation allowed by the plan. */
    size_t pathLen = 0u;
    while (pathLen < PNA_APUPDATE_PLAN_PATH_SIZE &&
           locallyGeneratedSourcePath[pathLen] != '\0') ++pathLen;
    if (pathLen == 0u || pathLen == PNA_APUPDATE_PLAN_PATH_SIZE ||
        strcmp(locallyGeneratedSourcePath, PNA_APUPDATE_INSTALL_PATH) == 0)
        return false;

    /* Defensive consistency check: all inner byte ranges are contiguous. */
    uint64_t offset = PNA_APUPDATE_HEADER_LEN;
    for (size_t i = 0u; i < info->fileCount; ++i) {
        const PnaApupdateEntry* entry = &info->entries[i];
        if (entry->size == 0u || entry->offset != offset ||
            offset > info->packageSize ||
            entry->size > info->packageSize - offset) return false;
        offset += entry->size;
    }
    if (offset != info->packageSize) return false;

    /* The preview comes from pnaApupdateBuildPreview after all CRC checks.
     * Ensure it still looks like a NUL-padded legacy instruction header. */
    static const char prefix[] = "[namelist]\r\n";
    if (previewTextBytes < sizeof(prefix) - 1u ||
        memcmp(preparedHeader4096, prefix, sizeof(prefix) - 1u) != 0 ||
        preparedHeader4096[previewTextBytes] != 0u) return false;
    for (size_t i = 0u; i < previewTextBytes; ++i) {
        if (preparedHeader4096[i] == 0u) return false;
    }
    for (size_t i = previewTextBytes; i < PNA_APUPDATE_HEADER_LEN; ++i) {
        if (preparedHeader4096[i] != 0u) return false;
    }

    PnaApupdatePreparePlan plan = {0};
    memcpy(plan.source, locallyGeneratedSourcePath, pathLen);
    memcpy(plan.destination, PNA_APUPDATE_INSTALL_PATH,
           sizeof(PNA_APUPDATE_INSTALL_PATH));
    plan.packageBytes = info->packageSize;
    plan.headerRewriteBytes = PNA_APUPDATE_HEADER_LEN;
    plan.headerPreviewTextBytes = (uint32_t)previewTextBytes;
    plan.headerPreviewCrc32 = pnaApupdateCrc32(
        UINT32_MAX, preparedHeader4096, PNA_APUPDATE_HEADER_LEN) ^ UINT32_MAX;
    plan.embeddedFileCount = info->fileCount;
    plan.sourceReverified = true;
    plan.embeddedCrcsVerified = true;
    /* Capacity, destination ownership and publisher trust are UNVERIFIED.
     * Keep all execution capabilities disabled. */
    *out = plan;
    return true;
}
