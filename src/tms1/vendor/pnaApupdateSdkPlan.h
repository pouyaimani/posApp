#ifndef PNA_APUPDATE_SDK_PLAN_H_
#define PNA_APUPDATE_SDK_PLAN_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "pnaApupdatePlan.h"

/*
 * Stage 15A: SDK-specific, NON-EXECUTABLE storage sequence.
 * The actual device SDK signatures are:
 *   sdkFileInsert(char *path, u8 *source, u32 offset, u32 length)
 *   sdkFileWrite(char *path, u8 *source, u32 length)
 *   sdkFileDel(char *path)
 *
 * This module intentionally has NO declarations, function pointers, linking,
 * or invocations of those SDK functions. Do not use its result as permission
 * to touch APUPDATE, set update flags, or reboot.
 */
#define PNA_APUPDATE_SDK_CHUNK_BYTES UINT32_C(1024)

typedef struct {
    uint64_t copyBytes;
    uint32_t insertCalls;         /* Future sdkFileInsert operations, NOT executed. */
    uint32_t insertChunkBytes;    /* Maximum bytes per SDK insert, always <= 1024. */
    uint32_t lastInsertBytes;     /* Bytes in the final planned insert. */
    uint32_t headerWriteBytes;    /* One future sdkFileWrite of 4096 bytes. */
    uint32_t headerPreviewCrc32;
    uint32_t plannedReadbackBytes;/* Full package, then again after header rewrite. */
    uint8_t embeddedFileCount;
    bool requireSdkDeleteOrErasePolicy;
    bool requireSdkWriteSemanticsEvidence;
    bool requirePendingFlagCheck;
    bool requireCapacityCheck;
    bool requirePathAliasCheck;   /* APUPDATE == /mtd2/apupdate unverified. */
    bool requirePublisherAuth;
    bool destinationOwnerConfirmed;
    bool permissionToWrite;
    bool permissionToSetFlag;
    bool permissionToReboot;
    bool allChecksSatisfied;
    uint32_t executedSdkCalls;
} PnaApupdateSdkDryRun;

/* Pure function; validates the Stage 13 plan and derives bounded SDK steps.
 * `preparedHeader4096` is the already-validated INI preview from Stage 12.
 * This does not perform I/O or copy arbitrary source pointers into a plan.
 */
bool pnaApupdateSdkPlanBuild(const PnaApupdatePreparePlan* plan,
                             const uint8_t* preparedHeader4096,
                             size_t preparedCapacity,
                             PnaApupdateSdkDryRun* out);

#endif
