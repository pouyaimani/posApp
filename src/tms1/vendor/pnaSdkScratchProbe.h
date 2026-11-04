#ifndef PNA_SDK_SCRATCH_PROBE_H_
#define PNA_SDK_SCRATCH_PROBE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "file/file.h"

/* Stage 15B: scratch-only File HAL probe. Never accesses /mtd2/apupdate.
 * Uses the EXISTING file() HAL; no extra abstraction or SDK bridge. */
#define PNA_SDK_SCRATCH_PATH_SIZE 96u

typedef enum {
    PNA_SDK_SCRATCH_TAIL_UNKNOWN = 0,
    PNA_SDK_SCRATCH_TAIL_PRESERVED,
    PNA_SDK_SCRATCH_TAIL_UNREADABLE,
    PNA_SDK_SCRATCH_TAIL_PARTIAL,
    PNA_SDK_SCRATCH_TAIL_MODIFIED
} PnaSdkScratchTail;

typedef enum {
    PNA_SDK_SCRATCH_CHECK_ABSENT = 0,
    PNA_SDK_SCRATCH_OPEN_NEW,
    PNA_SDK_SCRATCH_INSERT_ZERO, /* HAL write: 16 bytes at offset 0 */
    PNA_SDK_SCRATCH_INSERT_OFFSET, /* HAL write: 16 bytes at offset 16 */
    PNA_SDK_SCRATCH_READ_ZERO,
    PNA_SDK_SCRATCH_READ_OFFSET,
    PNA_SDK_SCRATCH_REWRITE_HEAD,
    PNA_SDK_SCRATCH_READ_HEAD,
    PNA_SDK_SCRATCH_READ_TAIL,
    PNA_SDK_SCRATCH_CLOSE,
    PNA_SDK_SCRATCH_REMOVE,
    PNA_SDK_SCRATCH_CHECK_REMOVED,
    PNA_SDK_SCRATCH_COMPLETE,
    PNA_SDK_SCRATCH_FAILED
} PnaSdkScratchPhase;

typedef struct {
    char path[PNA_SDK_SCRATCH_PATH_SIZE];
    FileHandle* handle;
    PnaSdkScratchPhase phase;
    PnaSdkScratchTail tail;
    bool owned;     /* Attempted creation of a uniquely named scratch file. */
    bool cleanupOk;
    int lastHalCode;
    int failedPhase;
} PnaSdkScratchProbe;

typedef enum {
    PNA_SDK_SCRATCH_STEP_FAILED = -1,
    PNA_SDK_SCRATCH_STEP_BUSY = 0,
    PNA_SDK_SCRATCH_STEP_PASSED = 1
} PnaSdkScratchStepResult;

/* Only short /mtd0/tms_15b_<alnum,underscore,hyphen>.bin paths. */
bool pnaSdkScratchCanStart(const char* path);
bool pnaSdkScratchBegin(PnaSdkScratchProbe* probe, const char* path);
PnaSdkScratchStepResult pnaSdkScratchStep(PnaSdkScratchProbe* probe);
/* Call during failure/cancellation; never touches files outside scratch path. */
bool pnaSdkScratchAbort(PnaSdkScratchProbe* probe);

#endif
