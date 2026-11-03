#ifndef TMS_BRINGUP_H_
#define TMS_BRINGUP_H_

#include <stdbool.h>
#include <stdint.h>

#include "pnaTms.h"

/* Diagnostic firmware only. Real TmsMgr -> PNA backend -> NTH.
 * Caller must ensure financial and recovery flows are idle.
 * NTH is ticked exactly once by the existing application event loop.
 */

/* Stage 1: connection only; no TMS protocol packet is sent. */
bool tmsBringupStartConnectOnly(const char* host, uint16_t port, bool useTls);

/* Stage 2: send command 10 using the REAL identity and parse command 11.
 * Logs the validated manifest, then closes connection with a DIAGNOSTIC result.
 * NO sector requests, File HAL calls, or installation are permitted.
 * identity is copied into backend configuration before return.
 */
bool tmsBringupStartManifestOnly(const char* host, uint16_t port, bool useTls,
                                 const PnaTmsIdentity* identity);

/* Stage 3: real command 10 / 11, then request sector zero of the first
 * file with command 12, validate response 21 and stop. Does not open, write,
 * verify or install any files. May reconnect/retry on network errors.
 */
bool tmsBringupStartFirstSector(const char* host, uint16_t port, bool useTls,
                                const PnaTmsIdentity* identity);

/* Stage 4: request one validated sector, write it to a NEW scratch file,
 * flush, close/reopen, verify all bytes, and remove it. No installation.
 * scratchPrefix is required; the path PREFIX_00.bin must NOT exist.
 * Choose an existing writable location that contains no valuable data.
 */
bool tmsBringupStartFirstSectorWrite(const char* host, uint16_t port,
                                     bool useTls,
                                     const PnaTmsIdentity* identity,
                                     const char* scratchPrefix);

/* Stage 5: download ONLY the first manifest file completely; flush/sync,
 * reopen and CRC32-compare the whole file against received sector bytes.
 * Retains PREFIX_00.bin on PASS for inspection, never installs anything.
 * Refuses an existing path. Caller owns subsequent cleanup of the retained
 * test file, and must ensure sufficient free space before invoking. */
bool tmsBringupStartFullFile(const char* host, uint16_t port, bool useTls,
                             const PnaTmsIdentity* identity,
                             const char* scratchPrefix);

/* Stage 6: first file only; same download procedure, max 1024 bytes per
 * File HAL read and up to 16 reads per processing call, subject to a time
 * budget. Compare stored CRC32 to received
 * CRC32 and stored MD5 to the raw 16-byte server manifest MD5.
 * Retains the verified scratch on PASS; removes it on verification failure.
 * NEVER installs or activates a package. Use a NEW scratch prefix.
 */
bool tmsBringupStartIncrementalMd5(const char* host, uint16_t port,
                                   bool useTls,
                                   const PnaTmsIdentity* identity,
                                   const char* scratchPrefix);

/* Stage 7: download and incrementally verify EVERY file in the manifest.
 * Uses one PREFIX_XX.bin per file; refuses any existing destination.
 * Retains ALL files only if ALL CRC32/MD5 checks pass; on failure/cancel,
 * attempts to remove all staging files created by this run.
 * No installation. The File HAL supplied to this project has no free-space
 * query; provide a platform check through PnaTmsConfig.checkStorageSpace
 * when integrating a full capacity preflight. Use a NEW scratch prefix. */
bool tmsBringupStartAllFilesMd5(const char* host, uint16_t port,
                                bool useTls, const PnaTmsIdentity* identity,
                                const char* scratchPrefix);

/* Checked variant: device app provides a real free-space query.
 * The callback runs after the manifest is parsed, before any file is opened.
 * It must account for overhead/headroom in its own available-space policy. */
bool tmsBringupStartAllFilesMd5Checked(const char* host, uint16_t port,
                                       bool useTls,
                                       const PnaTmsIdentity* identity,
                                       const char* scratchPrefix,
                                       PnaTmsStorageSpaceCheck checkSpace,
                                       void* checkContext);

/* Stage 8: managed staging + restart-recoverable journal. An interrupted run
 * is cleaned automatically only when an intact INCOMPLETE journal proves
 * ownership. An existing READY package is NEVER overwritten automatically.
 * On success files and metadata are retained and reported READY, without
 * invoking installer or asserting publisher authenticity.
 * stagingPrefix: dedicated owned path, for example /mtd0/tms_managed_update
 * (do not reuse Stage 5/6/7 scratch prefixes).
 */
bool tmsBringupStartManagedReady(const char* host, uint16_t port,
                                 bool useTls, const PnaTmsIdentity* identity,
                                 const char* stagingPrefix);

/* Same as Stage 8 with an application-supplied free-space preflight.
 * The callback decides its own flash headroom and filesystem overhead. */
bool tmsBringupStartManagedReadyChecked(const char* host, uint16_t port,
                                        bool useTls,
                                        const PnaTmsIdentity* identity,
                                        const char* stagingPrefix,
                                        PnaTmsStorageSpaceCheck checkSpace,
                                        void* checkContext);

/* Inspect staged state after reboot. READY_UNTRUSTED needs a fresh hash and
 * publisher trust check before any later installer can consume it. */
PnaTmsStageStatus tmsBringupStage8Inspect(const char* stagingPrefix,
                                          PnaTmsStageInfo* info);

/* Explicit cleanup of a known managed prefix, including a READY package.
 * Only call when no TMS operation is active. */
bool tmsBringupStage8Discard(const char* stagingPrefix);

/* Invoke repeatedly from the application event loop AFTER the existing NTH tick. */
void tmsBringupProcess(void);

#endif
