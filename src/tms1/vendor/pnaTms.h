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

/* Compact TMS. Protected APUPDATE writes are optional and may trigger
 * platform installation automatically; there is no SDK flag/reboot call. */
#ifndef TMS_ENABLE_COPY_TO_SCRATCH
#define TMS_ENABLE_COPY_TO_SCRATCH 0
#endif
/* Optional read-only inspection of the existing updater slot.
 * OFF by default; no production footprint. No write/erase/activation calls. */
#ifndef TMS_ENABLE_APUPDATE_WRITER
#define TMS_ENABLE_APUPDATE_WRITER 1
#endif
#ifndef TMS_ENABLE_APUPDATE_AUDIT
#define TMS_ENABLE_APUPDATE_AUDIT 0
#endif
/* Optional SIGNATURE-only check during the existing read-only preflight.
 * Verify all embedded regions through System HAL; no update writes/flags.
 * Off by default to preserve the compact production footprint. */
#ifndef TMS_ENABLE_SIGNATURE_PREFLIGHT
#define TMS_ENABLE_SIGNATURE_PREFLIGHT 0
#endif
/* Test-POS ONLY: report sdkSysVerifySign failures while continuing the
 * read-only installer preflight. This does NOT authenticate a package.
 * Never compile this option with a destination writer or scratch copier. */
#ifndef TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT
#define TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT 0
#endif
/* Explicit DEVELOPMENT POS ONLY: omit SDK signature authentication while
 * testing protected destination writes. On tested POS firmware, writing
 * APUPDATE itself can trigger installation, WITHOUT any application call to
 * setUpdateFlag()/reboot(). This does NOT authenticate a publisher.
 * Keep both switches OFF in production builds. */
#ifndef TMS_DEV_POS_BUILD
#define TMS_DEV_POS_BUILD 1
#endif
#ifndef TMS_DEBUG_SKIP_SIGNATURE_FOR_PREPARE
#define TMS_DEBUG_SKIP_SIGNATURE_FOR_PREPARE 1
#endif
#if TMS_DEBUG_SKIP_SIGNATURE_FOR_PREPARE &&                                    \
    (!TMS_DEV_POS_BUILD || !TMS_ENABLE_APUPDATE_WRITER)
#error "Unsigned destination preparation requires DEV_POS_BUILD and WRITER"
#endif
#if TMS_DEBUG_SKIP_SIGNATURE_FOR_PREPARE && TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT
#error "Choose either unsigned read-only preflight or unsigned preparation"
#endif
#if TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT && !TMS_ENABLE_SIGNATURE_PREFLIGHT
#error                                                                         \
    "TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT requires TMS_ENABLE_SIGNATURE_PREFLIGHT=1"
#endif
#if TMS_DEBUG_ALLOW_UNSIGNED_PREFLIGHT &&                                      \
    (TMS_ENABLE_APUPDATE_WRITER || TMS_ENABLE_COPY_TO_SCRATCH)
#error "Unsigned debug preflight forbids updater and scratch writes"
#endif
#define PNA_TMS_STAGE_PATH_SIZE 128u

#if TMS_ENABLE_APUPDATE_WRITER
/* One-operation replacement decision, not a pending-update status query.
 * Invoked synchronously ONCE after READY integrity and the normal vendor
 * signature checks (omitted ONLY by an explicit dev-POS build), plus the
 * TMSclea/size precheck, immediately before modifying APUPDATE.
 * The implementer MUST enforce explicit operator authorization and any
 * applicable transaction/updater ownership requirements. Never return true
 * merely because the 7-byte clear marker exists.
 * The pointers are borrowed and valid only for the duration of the call.
 * MD5 binds the decision to the reverified READY package; it is not a
 * publisher-authentication mechanism. Normal builds require SDK signatures;
 * an explicitly enabled DEV-POS unsigned preparation does not authenticate
 * firmware; writing APUPDATE may itself activate the platform updater. */
typedef struct {
    uint64_t       operationId;
    const char*    destinationPath;
    const char*    stagedPath;
    const uint8_t* expectedMd5; /* 16 bytes, journal MD5 */
    uint32_t       stagedBytes;
} PnaTmsReplaceRequest;
typedef bool (*PnaTmsAuthorizeReplace)(void*                       context,
                                       const PnaTmsReplaceRequest* request);
#endif

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
    bool                   prepareDestination;
    PnaTmsAuthorizeReplace authorizeReplace; /* Explicit, single-use consent. */
    void*                  authorizationContext;
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
    bool replacementDecisionUsed; /* Refuse a repeated authorization attempt. */
#endif
#endif
} PnaTms;

bool       pnaTmsCopyField(void* dest, size_t destSize, const char* source,
                           size_t sourceCapacity);
bool       pnaTmsInit(PnaTms* context, const PnaTmsConfig* config);
TmsBackend pnaTmsBackend(PnaTms* context);

#endif /* PNA_TMS_H_ */
