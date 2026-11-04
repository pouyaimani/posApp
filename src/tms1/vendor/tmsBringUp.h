#ifndef TMS_BRINGUP_H_
#define TMS_BRINGUP_H_

#include <stdbool.h>
#include <stdint.h>
#include "pnaTms.h"

/* Compact staging and read-only installer preflight; no activation.
 * Caller must call nth()->tick() in its existing event loop before process().
 * A READY result is TMS_RESULT_SUCCESS, backendCode=8 (staging only).
 */
bool tmsBringupStartManagedReady(const char *host, uint16_t port, bool useTls,
                                 const PnaTmsIdentity *identity,
                                 const char *stagingPrefix);
bool tmsBringupStartManagedReadyChecked(const char *host, uint16_t port,
                                        bool useTls, const PnaTmsIdentity *identity,
                                        const char *stagingPrefix,
                                        PnaTmsStorageSpaceCheck checkSpace,
                                        void *checkContext);

PnaTmsStageStatus tmsBringupStage8Inspect(const char *stagingPrefix,
                                          PnaTmsStageInfo *info);
bool tmsBringupStage8Discard(const char *stagingPrefix);
/* Reverify READY outer MD5 + binary container + embedded CRCs. Generate the
 * 4096-byte legacy updater header in memory; never write to /mtd2/apupdate.
 * TMS_RESULT_SUCCESS / backendCode=9 means READ-ONLY PREFLIGHT, not installed.
 * Reuses the existing 100 ms tmsBringupProcess() loop. */
bool tmsBringupStartInstallerPreflight(const char *stagingPrefix);

/* Reverify READY then read-only inspect /mtd2/apupdate. Require the
 * 7-byte "TMSclea" marker, but allow and count non-FF residual payload.
 * Success means recognized marker and completed scan, NOT authorization.
 * Test-only: compile TMS_ENABLE_APUPDATE_AUDIT=1 consistently. */
#if defined(TMS_ENABLE_APUPDATE_AUDIT) && TMS_ENABLE_APUPDATE_AUDIT
bool tmsBringupStartApupdateAudit(const char *stagingPrefix);
#endif

/* Protected updater preparation is compiled OUT by default.
 * Only enable after providing SDK-backed updater-idle/ownership and
 * independently trusted publisher checks. No flag/reboot is ever called.
 * Does not remove the destination on failure. Code 12 = PREPARED only. */
#if TMS_ENABLE_APUPDATE_WRITER
bool tmsBringupStartApupdatePrepare(const char *stagingPrefix,
                                    PnaTmsSafetyCheck slotIdleAndOwned,
                                    PnaTmsPublisherCheck publisherTrusted,
                                    void *safetyContext);
#endif

/* One-off full-package COPY to a disposable /mtd0 file. Compile out of
 * production. It verifies readback and deletes ONLY the test-owned file.
 * Never accesses /mtd2/apupdate, never sets update flag or reboots. */
#if defined(TMS_ENABLE_COPY_TO_SCRATCH) && TMS_ENABLE_COPY_TO_SCRATCH
bool tmsBringupStartScratchCopy(const char *stagingPrefix);
#endif

/* One-off File HAL scratch test for header-first writes.
 * Define TMS_ENABLE_HEADER_FIRST_PROBE=1 only in a test build.
 * Does not touch the updater partition and is excluded from production. */
#if defined(TMS_ENABLE_HEADER_FIRST_PROBE) && TMS_ENABLE_HEADER_FIRST_PROBE
bool tmsBringupProbeHeaderFirst(void);
#endif

void tmsBringupProcess(void);

#endif
