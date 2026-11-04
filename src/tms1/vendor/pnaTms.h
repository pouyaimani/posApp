#ifndef PNA_TMS_H_
#define PNA_TMS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../tmsBackend.h"
#include "pnaTmsProtocol.h"
#include "pnaTmsMd5.h"
#include "pnaTmsStage.h"
#include "nth/nth.h"
#include "file/file.h"

/* Compact download and read-only installer preflight. No activation. */
#ifndef TMS_ENABLE_COPY_TO_SCRATCH
#define TMS_ENABLE_COPY_TO_SCRATCH 0
#endif
/* Optional read-only inspection of the existing updater slot.
 * OFF by default; no production footprint. No write/erase/activation calls. */
#ifndef TMS_ENABLE_APUPDATE_WRITER
#define TMS_ENABLE_APUPDATE_WRITER 0
#endif
#ifndef TMS_ENABLE_APUPDATE_AUDIT
#define TMS_ENABLE_APUPDATE_AUDIT 1
#endif
#define PNA_TMS_STAGE_PATH_SIZE 128u

/* Platform-provided checks, NOT File HAL wrappers. Never substitute the
 * legacy 7-byte marker for a pending-update/ownership query or trust check. */
typedef bool (*PnaTmsSafetyCheck)(void* context);
/* Must independently authenticate publisher signature for exactly this source.
 * The staged MD5 is for equality only; it is NOT publisher authentication. */
typedef bool (*PnaTmsPublisherCheck)(void* context, const char* stagedPath,
                                     const uint8_t expectedMd5[16],
                                     uint32_t      stagedBytes);

typedef bool (*PnaTmsStorageSpaceCheck)(void* context, const char* prefix,
                                        uint64_t requiredBytes);

typedef struct {
    char           host[64];
    uint16_t       port;
    bool           tls; /* Unsupported by the current NTH; fail closed. */
    PnaTmsIdentity identity;
    char           stagingPrefix[96];
    uint32_t       ioTimeoutMs;
    uint8_t        maxSectorAttempts; /* Zero defaults to 3. */
    bool           requireEndPacket;
    bool prepareOnly; /* Offline: always reverify READY and its embedded files.
                       */
#if TMS_ENABLE_APUPDATE_WRITER
    bool prepareDestination;
    PnaTmsSafetyCheck
        slotIdleAndOwned; /* Real platform updater status check. */
    PnaTmsPublisherCheck
          publisherTrusted; /* Real cryptographic publisher check. */
    void* safetyContext;
#endif
#if TMS_ENABLE_APUPDATE_AUDIT
    bool auditApupdate; /* Inspect /mtd2/apupdate as read-only storage. */
#endif
#if TMS_ENABLE_COPY_TO_SCRATCH
    bool copyToScratch; /* Explicit opt-in disposable /mtd0 copy test ONLY. */
#endif
    PnaTmsStorageSpaceCheck checkStorageSpace; /* Optional preflight. */
    void*                   storageSpaceContext;
} PnaTmsConfig;

typedef enum {
    PNA_TMS_IDLE = 0,
    PNA_TMS_CONNECTING,
    PNA_TMS_WAIT_RESPONSE,
    PNA_TMS_VERIFY_FILE,
    PNA_TMS_WAIT_END,
    PNA_TMS_PREPARE_MD5,
    PNA_TMS_PREPARE_HEADER,
    PNA_TMS_PREPARE_PAYLOAD,
#if TMS_ENABLE_APUPDATE_AUDIT
    PNA_TMS_AUDIT_APUPDATE,
#endif
#if TMS_ENABLE_COPY_TO_SCRATCH || TMS_ENABLE_APUPDATE_WRITER
    PNA_TMS_SCRATCH_COPY,
    PNA_TMS_SCRATCH_VERIFY_HEADER,
    PNA_TMS_SCRATCH_VERIFY_PAYLOAD,
#endif
} PnaTmsStage;

/* Legacy 4096-byte apupdate container (raw CRC, not a signature). */
typedef struct {
    char     name[21];
    uint32_t offset;
    uint32_t size;
    uint32_t rawCrc;
} PnaTmsInnerFile;

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
    uint8_t
        rx[PNA_TMS_MAX_FRAME_SIZE]; /* Reused as 1024-byte MD5 read buffer. */
    size_t  rxLength;
    size_t  rxOffset;
    uint8_t request[PNA_TMS_RESUME_SIZE];

    PnaTmsFile      files[PNA_TMS_MAX_FILES];
    PnaTmsStageInfo journal;
    FileHandle*     opened;
    char            currentPath[PNA_TMS_STAGE_PATH_SIZE];
    size_t          fileCount;
    size_t          fileIndex;
    uint16_t        sectorIndex;
    uint8_t         sectorAttempt;
    bool            manifestReceived;
    bool            journalOwned;
    bool            verifyStarted;
    bool            endReceived;
    uint64_t        totalBytes;
    uint64_t        downloadedBytes;
    uint64_t        verifiedBytes;
    uint32_t        downloadedCrc;
    uint32_t        verifyCrc;
    uint32_t        verifyOffset;
    PnaTmsMd5       verifyMd5;
    uint8_t         innerCount;
    PnaTmsInnerFile inner[15];
#if TMS_ENABLE_APUPDATE_AUDIT
    uint32_t auditSize;   /* Reported physical slot size. */
    uint32_t auditOffset; /* Bytes examined, 1024 or less per HAL read. */
    uint32_t
        auditResidualBytes; /* Non-FF bytes after the 7-byte clear marker. */
#endif
#if TMS_ENABLE_COPY_TO_SCRATCH || TMS_ENABLE_APUPDATE_WRITER
    FileHandle* scratchSource;
    uint32_t    scratchOffset;
    uint32_t    scratchHeaderCrc;
    bool        scratchOwned;
#if TMS_ENABLE_APUPDATE_WRITER
    bool destinationTouched;
#endif
#endif
} PnaTms;

bool       pnaTmsCopyField(void* dest, size_t destSize, const char* source,
                           size_t sourceCapacity);
bool       pnaTmsInit(PnaTms* context, const PnaTmsConfig* config);
TmsBackend pnaTmsBackend(PnaTms* context);

#endif /* PNA_TMS_H_ */
