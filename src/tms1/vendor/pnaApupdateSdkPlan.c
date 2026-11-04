#include "pnaApupdateSdkPlan.h"

#include <limits.h>
#include <string.h>

/* Check strings without reading past their containing fixed-size arrays. */
static bool hasTerminator(const char* s, size_t cap) {
    return s && memchr(s, '\0', cap) != NULL;
}

bool pnaApupdateSdkPlanBuild(const PnaApupdatePreparePlan* plan,
                             const uint8_t*                preparedHeader4096,
                             size_t                        preparedCapacity,
                             PnaApupdateSdkDryRun*         out) {
    if (!plan || !preparedHeader4096 || !out ||
        preparedCapacity < PNA_APUPDATE_HEADER_LEN || !plan->sourceReverified ||
        !plan->embeddedCrcsVerified ||
        plan->packageBytes <= PNA_APUPDATE_HEADER_LEN ||
        plan->packageBytes > UINT32_MAX ||
        plan->headerRewriteBytes != PNA_APUPDATE_HEADER_LEN ||
        plan->headerPreviewTextBytes == 0u ||
        plan->headerPreviewTextBytes >= PNA_APUPDATE_HEADER_LEN ||
        plan->embeddedFileCount == 0u ||
        plan->embeddedFileCount > PNA_APUPDATE_MAX_FILES ||
        !hasTerminator(plan->source, sizeof(plan->source)) ||
        !hasTerminator(plan->destination, sizeof(plan->destination)) ||
        strcmp(plan->destination, PNA_APUPDATE_INSTALL_PATH) != 0 ||
        strcmp(plan->source, plan->destination) == 0 ||
        plan->publisherAuthenticated || plan->installationAuthorized ||
        plan->destinationSpaceConfirmed ||
        plan->destinationOwnershipConfirmed || plan->writesEnabled ||
        plan->flagEnabled || plan->rebootEnabled)
        return false;

    /* Verify the 4096-byte INI image has not changed since Stage 13. */
    static const uint8_t signature[] = "[namelist]\r\n";
    if (memcmp(preparedHeader4096, signature, sizeof(signature) - 1u) != 0 ||
        preparedHeader4096[plan->headerPreviewTextBytes] != 0u)
        return false;
    for (size_t i = plan->headerPreviewTextBytes; i < PNA_APUPDATE_HEADER_LEN;
         ++i)
        if (preparedHeader4096[i] != 0u)
            return false;
    const uint32_t crc = pnaApupdateCrc32(UINT32_MAX, preparedHeader4096,
                                          PNA_APUPDATE_HEADER_LEN) ^
                         UINT32_MAX;
    if (crc != plan->headerPreviewCrc32)
        return false;

    const uint64_t calls =
        (plan->packageBytes + PNA_APUPDATE_SDK_CHUNK_BYTES - 1u) /
        PNA_APUPDATE_SDK_CHUNK_BYTES;
    if (calls == 0u || calls > UINT32_MAX)
        return false;

    PnaApupdateSdkDryRun draft = {0};
    draft.copyBytes            = plan->packageBytes;
    draft.insertCalls          = (uint32_t)calls;
    draft.insertChunkBytes     = PNA_APUPDATE_SDK_CHUNK_BYTES;
    draft.lastInsertBytes =
        (uint32_t)((plan->packageBytes - 1u) % PNA_APUPDATE_SDK_CHUNK_BYTES) +
        1u;
    draft.headerWriteBytes                 = PNA_APUPDATE_HEADER_LEN;
    draft.headerPreviewCrc32               = crc;
    draft.plannedReadbackBytes             = (uint32_t)plan->packageBytes;
    draft.embeddedFileCount                = plan->embeddedFileCount;
    draft.requireSdkDeleteOrErasePolicy    = true;
    draft.requireSdkWriteSemanticsEvidence = true;
    draft.requirePendingFlagCheck          = true;
    draft.requireCapacityCheck             = true;
    draft.requirePathAliasCheck            = true;
    draft.requirePublisherAuth             = true;
    /* Every execution/authorization field stays false, always. */
    *out = draft;
    return true;
}