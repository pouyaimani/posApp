#ifndef PNA_TMS_H_
#define PNA_TMS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../tmsBackend.h"
#include "pnaTmsProtocol.h"
#include "nth/nth.h"
#include "file/file.h"

/* Enable only in diagnostic firmware builds. Production default is disabled. */
#ifndef PNA_TMS_ENABLE_DIAGNOSTICS
#define PNA_TMS_ENABLE_DIAGNOSTICS 1
#endif

#if PNA_TMS_ENABLE_DIAGNOSTICS
#include "pnaTmsMd5.h"
#include "pnaTmsStage.h"
/* Optional device-specific available-storage query for Stage 7. */
typedef bool (*PnaTmsStorageSpaceCheck)(void*       context,
                                        const char* stagingPrefix,
                                        uint64_t    requiredBytes);
typedef enum {
    PNA_TMS_CHECKPOINT_NONE      = 0,
    PNA_TMS_CHECKPOINT_CONNECTED = 1,
    /* Transmit command 10, validate/inspect command 11, then STOP. */
    PNA_TMS_CHECKPOINT_MANIFEST = 2,
    /* Send command 12 for file 0 sector 0, validate response 21; never write.
     */
    PNA_TMS_CHECKPOINT_FIRST_SECTOR = 3,
    /* Write sector 0 to an unused scratch file, verify readback, delete. */
    PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE = 4,
    /* Download file 0 fully to a new scratch file; verify readback; no install.
     */
    PNA_TMS_CHECKPOINT_FULL_FILE = 5,
    /* Download first file, then cooperatively verify CRC32 + manifest MD5. */
    PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 = 6,
    /* Download and verify every manifest file; retain only on full success. */
    PNA_TMS_CHECKPOINT_ALL_FILES_MD5 = 7,
    /* Managed journal and all-file MD5; ready only, NEVER install. */
    PNA_TMS_CHECKPOINT_MANAGED_READY = 8
} PnaTmsCheckpoint;
#endif

/* No duplicate NTH/File transport interface: PNA calls those services directly.
 */
#define PNA_TMS_STAGE_PATH_SIZE 128u

/*
 * These hooks describe PACKAGE VERIFICATION / INSTALLATION, not file I/O.
 * Verification must authenticate application/firmware packages using an
 * approved trust policy. The 16-byte manifest 'MD5' field by itself cannot
 * authenticate executable updates; its real encoding is unconfirmed.
 *
 * Installation hooks must return promptly. For a blocking device updater,
 * start a worker/task and marshal completion to the same execution context.
 * beginInstall/pollInstall: >0 completed, 0 in progress, <0 failed.
 * A successful install means the installer no longer needs temporary files,
 * or it has safely taken responsibility for their durability and ownership.
 */
typedef struct {
    void* context;
    bool (*verify)(void* context, const PnaTmsFile* file,
                   const char* stagingPath);
    int (*beginInstall)(void* context, const PnaTmsFile* files,
                        const char paths[][PNA_TMS_STAGE_PATH_SIZE],
                        size_t     count);
    int (*pollInstall)(void* context);
} PnaTmsPackageHooks;

typedef struct {
    char     host[64];
    uint16_t port;
    bool     tls; /* Refused until the existing NTH supports TLS explicitly. */
    PnaTmsIdentity identity;
    char stagingPrefix[96]; /* e.g. "tms/stage"; paths become PREFIX_00.bin */
    /* Stage-4 diagnostics require a dedicated, unused scratch prefix.
     * The backend refuses to overwrite an existing scratch file. */
    PnaTmsPackageHooks package;
    uint32_t ioTimeoutMs;       /* 0: NTH default. NTH owns socket timeouts. */
    uint8_t  maxSectorAttempts; /* 0: 3, including original attempt. */
    bool requireEndPacket; /* Wait for server command 22, if deployment uses it.
                            */
#if PNA_TMS_ENABLE_DIAGNOSTICS
    /* Test-only checkpoint. MANIFEST sends command 10 but never downloads. */
    PnaTmsCheckpoint checkpoint;
    /* Optional platform capacity check. Return true only when at least
     * requiredBytes of writable space exists for stagingPrefix. Stage 7
     * checks this BEFORE opening any staging file. If NULL, the backend
     * still enforces path collision protection and fails safely on write
     * errors, but cannot claim a free-space preflight was performed. */
    PnaTmsStorageSpaceCheck checkStorageSpace;
    void*                   storageSpaceContext;
#endif
} PnaTmsConfig;

typedef enum {
    PNA_TMS_IDLE = 0,
    PNA_TMS_CONNECTING,
    PNA_TMS_WAIT_RESPONSE,
    PNA_TMS_VERIFY_FILE,
    PNA_TMS_WAIT_END,
    PNA_TMS_INSTALL_START,
    PNA_TMS_INSTALL_WAIT
} PnaTmsStage;

/* Caller-owned stable object (heap allocation is supported). Never copy while
 * active. */
typedef struct {
    PnaTmsConfig     cfg;
    TmsBackendNotify notify;
    void*            notifyContext;
    uint64_t         operationId;
    bool             active;
    PnaTmsStage      stage;

    NthTransaction* tx;
    bool            frameReady;
    bool            frameBad;
    bool            netFailed;
    NthResult       netError;
    uint8_t         rx[PNA_TMS_MAX_FRAME_SIZE];
    size_t          rxLength; /* Expected full frame size, initially 2. */
    size_t          rxOffset;
    uint8_t         request[PNA_TMS_RESUME_SIZE];

    PnaTmsFile files[PNA_TMS_MAX_FILES];
    size_t     fileCount;
    size_t     fileIndex;
    uint16_t   sectorIndex;
    uint8_t    sectorAttempt;
    bool       manifestReceived;
    uint64_t   totalBytes;
    uint64_t   downloadedBytes;

    FileHandle* opened;
    char        stagedPaths[PNA_TMS_MAX_FILES][PNA_TMS_STAGE_PATH_SIZE];
    size_t      stagedCount;
    bool        installationStarted;
#if PNA_TMS_ENABLE_DIAGNOSTICS
    uint32_t  diagnosticCrc; /* Rolling CRC32 of downloaded data. */
    bool      verifyStarted;
    uint32_t  verifyOffset;
    uint32_t  verifyCrc;
    PnaTmsMd5 verifyMd5;
    uint64_t
        verifiedBytes; /* Total bytes belonging to previously verified files. */
    PnaTmsStageInfo stage8Journal; /* Persistent managed-staging metadata. */
    bool stage8JournalStarted;     /* Current run owns an INCOMPLETE journal. */
#endif
} PnaTms;

/* Copy a bounded NUL-terminated source into a fixed-width, space-padded
 * wire field. Reject truncation rather than silently losing protocol bytes.
 * Kept in pnaTms.c, matching the device application's current helper. */
bool pnaTmsCopyField(void* dst, size_t dstSize, const char* src,
                     size_t srcCapacity);

bool       pnaTmsInit(PnaTms* context, const PnaTmsConfig* config);
TmsBackend pnaTmsBackend(PnaTms* context);

#endif
