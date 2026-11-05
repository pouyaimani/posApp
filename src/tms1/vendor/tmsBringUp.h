#ifndef TMS_BRINGUP_H_
#define TMS_BRINGUP_H_

#include <stdbool.h>
#include <stdint.h>
#include "pnaTms.h"

/* End-to-end DEV flow. CHECK and DOWNLOAD are stages of the PNA backend's
 * online run; PREPARING is a second, offline run. SUCCESS+code=8 means READY
 * only, while code=13 in the current DEV build means updater prepared.
 * Neither is proof that the new firmware has booted. */
typedef enum {
    TMS_FULL_IDLE = 0,
    TMS_FULL_CHECKING,
    TMS_FULL_PREPARE_QUEUED,
    TMS_FULL_PREPARING,
    TMS_FULL_ACTIVATION_QUEUED,
    TMS_FULL_REBOOT_REQUESTED,
    TMS_FULL_NO_UPDATE,
    TMS_FULL_FAILED
} TmsFullStage;

typedef struct {
    const char*           host;
    uint16_t              port;
    bool                  useTls;
    const PnaTmsIdentity* identity;      /* Copied synchronously at start. */
    const char*           stagingPrefix; /* Copied synchronously at start. */
#if TMS_ENABLE_APUPDATE_WRITER
    PnaTmsAuthorizeReplace authorizeReplace;
#else
    void* authorizeReplace;
#endif
    void* authorizationContext; /* Must remain valid until preparation ends. */
    PnaTmsStorageSpaceCheck checkStorageSpace;
    void* storageSpaceContext;  /* Must remain valid during download. */
    bool  discardExistingReady; /* Explicitly remove old READY, DEV test. */
} TmsFullConfig;

typedef struct {
    TmsFullStage stage;
    TmsStatus    last; /* Current online/offline result, not post-boot proof. */
} TmsFullStatus;

bool          tmsBringupStartFullUpgrade(const TmsFullConfig* config);
TmsFullStatus tmsBringupGetFullStatus(void);

/* Compact TMS bring-up adapter. The caller must call nth()->tick() in its
 * existing event loop before process(). READY success (backendCode=8) never
 * activates the updater. Only a successful protected destination preparation
 * (backendCode=12, or code=13 on an unsigned DEVELOPMENT POS) schedules the
 * activation sequence implemented in tmsBringupProcess().
 */
bool tmsBringupStartManagedReady(const char* host, uint16_t port, bool useTls,
                                 const PnaTmsIdentity* identity,
                                 const char*           stagingPrefix);
bool tmsBringupStartManagedReadyChecked(const char* host, uint16_t port,
                                        bool                    useTls,
                                        const PnaTmsIdentity*   identity,
                                        const char*             stagingPrefix,
                                        PnaTmsStorageSpaceCheck checkSpace,
                                        void*                   checkContext);

PnaTmsStageStatus tmsBringupStage8Inspect(const char*      stagingPrefix,
                                          PnaTmsStageInfo* info);
bool              tmsBringupStage8Discard(const char* stagingPrefix);
/* Reverify READY outer MD5 + binary container + embedded CRCs. Generate the
 * 4096-byte legacy updater header in memory; never write to /mtd2/apupdate.
 * TMS_RESULT_SUCCESS / backendCode=9 means READ-ONLY PREFLIGHT, not installed.
 * Reuses the existing 100 ms tmsBringupProcess() loop. */
bool tmsBringupStartInstallerPreflight(const char* stagingPrefix);

/* Reverify READY then read-only inspect /mtd2/apupdate. Require the
 * 7-byte "TMSclea" marker, but allow and count non-FF residual payload.
 * Success means recognized marker and completed scan, NOT authorization.
 * Test-only: compile TMS_ENABLE_APUPDATE_AUDIT=1 consistently. */
#if defined(TMS_ENABLE_APUPDATE_AUDIT) && TMS_ENABLE_APUPDATE_AUDIT
bool tmsBringupStartApupdateAudit(const char* stagingPrefix);
#endif

/* Protected updater preparation is compiled OUT by default.
 * Requires a synchronous, once-per-operation replacement decision bound to
 * the verified READY MD5/size and operation ID. The callback must enforce
 * explicit operator consent and applicable platform ownership constraints.
 * SDK signatures are mandatory in production. An explicitly opted-in
 * DEV-POS unsigned test build skips signatures but returns code 13 instead of
 * authenticated PREPARED code 12. This bring-up adapter requests the platform
 * update flag and reboot only after that preparation succeeds and all backend
 * resources have been released. Code 13 is for controlled DEV-POS tests only;
 * never enable unsigned activation on a production/payment terminal. */
#if TMS_ENABLE_APUPDATE_WRITER
bool tmsBringupStartApupdatePrepare(const char*            stagingPrefix,
                                    PnaTmsAuthorizeReplace authorizeReplace,
                                    void* authorizationContext);
#endif

/* One-off full-package COPY to a disposable /mtd0 file. Compile out of
 * production. It verifies readback and deletes ONLY the test-owned file.
 * Never accesses /mtd2/apupdate, never sets update flag or reboots. */
#if defined(TMS_ENABLE_COPY_TO_SCRATCH) && TMS_ENABLE_COPY_TO_SCRATCH
bool tmsBringupStartScratchCopy(const char* stagingPrefix);
#endif

/* One-off File HAL scratch test for header-first writes.
 * Define TMS_ENABLE_HEADER_FIRST_PROBE=1 only in a test build.
 * Does not touch the updater partition and is excluded from production. */
#if defined(TMS_ENABLE_HEADER_FIRST_PROBE) && TMS_ENABLE_HEADER_FIRST_PROBE
bool tmsBringupProbeHeaderFirst(void);
#endif

void tmsBringupProcess(void);

#endif
