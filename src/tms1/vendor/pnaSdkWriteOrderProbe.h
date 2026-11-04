#ifndef PNA_SDK_WRITE_ORDER_PROBE_H_
#define PNA_SDK_WRITE_ORDER_PROBE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "file/file.h"

/* Stage 15C: opt-in scratch-only File HAL test; NEVER /mtd2/apupdate.
 * The same short, previously absent path is used for two separate trials.
 * The first trial must be removed successfully before the second starts. */
#define PNA_SDK_ORDER_PATH_SIZE 48u

typedef enum {
    PNA_SDK_ORDER_NOT_TESTED = 0,
    PNA_SDK_ORDER_MATCH = 1,
    PNA_SDK_ORDER_CHANGED = 2,
    PNA_SDK_ORDER_UNREADABLE = 3,
    PNA_SDK_ORDER_PARTIAL = 4
} PnaSdkOrderReadResult;

typedef enum {
    PNA_SDK_ORDER_CHECK_ABSENT = 0,
    PNA_SDK_ORDER_A_OPEN,
    PNA_SDK_ORDER_A_HEADER_FIRST,
    PNA_SDK_ORDER_A_APPEND_PAYLOAD,
    PNA_SDK_ORDER_A_VERIFY,
    PNA_SDK_ORDER_A_CLOSE,
    PNA_SDK_ORDER_A_DELETE,
    PNA_SDK_ORDER_A_CHECK_DELETED,
    PNA_SDK_ORDER_B_OPEN,
    PNA_SDK_ORDER_B_WRITE_HEADER,
    PNA_SDK_ORDER_B_APPEND_PAYLOAD,
    PNA_SDK_ORDER_B_VERIFY_INITIAL,
    PNA_SDK_ORDER_B_SEEK_HEADER,
    PNA_SDK_ORDER_B_REPLACE_HEADER,
    PNA_SDK_ORDER_B_VERIFY_FINAL,
    PNA_SDK_ORDER_B_CLOSE,
    PNA_SDK_ORDER_B_DELETE,
    PNA_SDK_ORDER_B_CHECK_DELETED,
    PNA_SDK_ORDER_COMPLETE,
    PNA_SDK_ORDER_FAILED
} PnaSdkOrderPhase;

typedef enum {
    PNA_SDK_ORDER_STEP_FAILED = -1,
    PNA_SDK_ORDER_STEP_BUSY = 0,
    PNA_SDK_ORDER_STEP_DONE = 1
} PnaSdkOrderStepResult;

typedef struct {
    char path[PNA_SDK_ORDER_PATH_SIZE];
    FileHandle* handle;
    PnaSdkOrderPhase phase;
    bool owned;
    bool cleanupOk;
    bool initialBVerified;
    bool viableA;
    bool viableB;
    PnaSdkOrderReadResult aHeader;
    PnaSdkOrderReadResult aPayload;
    PnaSdkOrderReadResult bHeader;
    PnaSdkOrderReadResult bPayload;
    long aSize;
    long bSizeBefore;
    long bSizeAfter;
    int lastHalCode;
    int failedPhase;
} PnaSdkWriteOrderProbe;

/* Short /mtd0/tms_15c_<identifier>.bin files only (max 31 characters).
 * Must refer to a unique absent file; never recycle an existing path. */
bool pnaSdkOrderCanStart(const char* path);
bool pnaSdkOrderBegin(PnaSdkWriteOrderProbe* probe, const char* path);
PnaSdkOrderStepResult pnaSdkOrderStep(PnaSdkWriteOrderProbe* probe);
/* Cleanup is allowed only after Begin has validated the owned path. */
bool pnaSdkOrderAbort(PnaSdkWriteOrderProbe* probe);

#endif
