#ifndef TMS_BRINGUP_H_
#define TMS_BRINGUP_H_

#include <stdbool.h>
#include <stdint.h>
#include "pnaTms.h"

// #define TMS_ENABLE_HEADER_FIRST_PROBE 1

/* Compact staging and read-only installer preflight; no activation.
 * Caller must call nth()->tick() in its existing event loop before process().
 * A READY result is TMS_RESULT_SUCCESS, backendCode=8 (staging only).
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

/* One-off File HAL scratch test for header-first writes.
 * Define TMS_ENABLE_HEADER_FIRST_PROBE=1 only in a test build.
 * Does not touch the updater partition and is excluded from production. */
#if defined(TMS_ENABLE_HEADER_FIRST_PROBE) && TMS_ENABLE_HEADER_FIRST_PROBE
bool tmsBringupProbeHeaderFirst(void);
#endif

void tmsBringupProcess(void);

#endif
