#ifndef PNA_TMS_STAGE_H_
#define PNA_TMS_STAGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pnaTmsProtocol.h"

/*
 * Stage 8 durable staging journal, independent from NTH and TmsMgr.
 * Two alternating, checksummed records avoid trusting a torn metadata write.
 * This is a crash-recovery best effort with the currently available File HAL;
 * actual power-loss guarantees depend on the filesystem's sync semantics.
 *
 * READY means the files passed manifest-MD5 integrity verification in the
 * previous execution. It is NOT publisher authentication, and on a reboot
 * content MUST be re-verified before any installation policy can trust it.
 */
typedef enum {
    PNA_TMS_STAGE_EMPTY = 0,
    PNA_TMS_STAGE_INCOMPLETE,
    PNA_TMS_STAGE_READY_UNTRUSTED,
    PNA_TMS_STAGE_CORRUPT,
    PNA_TMS_STAGE_IO_ERROR
} PnaTmsStageStatus;

typedef struct {
    uint32_t sequence;
    uint16_t fileCount;
    uint16_t verifiedCount;
    uint64_t totalBytes;
    PnaTmsFile files[PNA_TMS_MAX_FILES];
    PnaTmsStageStatus status;
} PnaTmsStageInfo;

/* Inspect journal only; never trusts an unverified file or deletes anything.
 * Returns EMPTY/INCOMPLETE/READY_UNTRUSTED/CORRUPT/IO_ERROR. If info is not
 * NULL it receives the last valid manifest on INCOMPLETE or READY. */
PnaTmsStageStatus pnaTmsStageInspect(const char* prefix,
                                     PnaTmsStageInfo* info);

/* Build PREFIX_XX.bin without accepting server filenames as paths. */
bool pnaTmsStageFilePath(const char* prefix, size_t index,
                         char* output, size_t capacity);

/* Start a new journal only when EMPTY. No file is created by this function. */
bool pnaTmsStageBegin(const char* prefix, const PnaTmsFile* files,
                      size_t count, uint64_t totalBytes,
                      PnaTmsStageInfo* inOut);

/* Commit each file after durable close and complete MD5/readback verification. */
bool pnaTmsStageMarkVerified(const char* prefix, PnaTmsStageInfo* inOut,
                             size_t verifiedCount);

/* Only after ALL files were verified: persist READY before reporting PASS. */
bool pnaTmsStageMarkReady(const char* prefix, PnaTmsStageInfo* inOut);

/* Idempotent recovery: delete only deterministic files referenced by a valid
 * INCOMPLETE journal, then both journal slots. READY packages are NEVER
 * deleted unless the caller explicitly uses discardReady=true. A corrupt
 * journal cannot authorize deletion of any data. */
bool pnaTmsStageDiscard(const char* prefix, bool discardReady);

#endif
