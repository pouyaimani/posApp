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

/* Stage 9: AFTER REBOOT, re-hash the existing Stage 8 managed files without
 * contacting TMS or writing/deleting any staged data or journal. Each file
 * is checked against its manifest MD5 with bounded 1024-byte File HAL reads.
 * Success is an in-memory diagnostic result (code=9); it is NOT publisher
 * authentication or installation authorization. On mismatch, neither the
 * files nor journal are automatically deleted. Requires an existing READY
 * Stage 8 journal at this exact prefix.
 */
bool tmsBringupStartRestartReverify(const char* stagingPrefix);

/* Stage 10: rehash Stage 8 READY files offline, then pass a file-by-file
 * plan to a read-only MOCK installer preflight. Never downloads, modifies,
 * authenticates publisher, installs, activates, or deletes any package.
 * Requires the EXACT Stage 8 prefix, no new scratch path.
 */
bool tmsBringupStartInstallerDryRun(const char* stagingPrefix);

/* Stage 11: rehash Stage 8 files, then check a package-authentication
 * provider and perform READ-ONLY installer preflight. Never installs.
 * A missing provider FAILS CLOSED (expected negative device test).
 */
bool tmsBringupStartAuthGate(const char* stagingPrefix);

/* Explicit SIMULATION for exercising successful gate wiring on hardware.
 * Reports diagnostic success but NEVER real publisher authentication or
 * installation authorization; NOT a production verifier. */
bool tmsBringupStartAuthGateSimulation(const char* stagingPrefix);

/* Integrator supplies a REAL platform signature/chain/compatibility verifier.
 * A true TRUSTED result depends on implementation of the provider. No real
 * verifier is bundled. This checkpoint still NEVER installs or persists auth.
 * The pointed-to provider is copied into backend config before return. */
bool tmsBringupStartAuthGateWithProvider(
    const char* stagingPrefix, const TmsPackageAuthenticator* provider);

/* Stage 12: offline and READ-ONLY. Reverify the existing Stage 8 managed
 * staging files, then inspect file 0 as a legacy 4096-byte apupdate container.
 * Strictly validate packed header, file bounds, and every embedded-file CRC;
 * construct (but NEVER write) the text updater header. Only one outer TMS
 * file is currently supported for this checkpoint. No /mtd2/apupdate I/O,
 * sdkSysSetUpdateFlag(), reboot, network, or installation. */
bool tmsBringupStartApupdateInspect(const char* stagingPrefix);

/* Stage 13: run Stage 12 offline compatibility checks and calculate an
 * exact dry-run plan for copying the verified container from managed staging
 * to /mtd2/apupdate, then replacing its first 4096 bytes with the generated
 * legacy INI instructions. Planning only: NO destination access, copying,
 * writes, update flag, reboot, installation or authorization. The supplied
 * File HAL provides no free-space query or atomic rename. Stage 13 reports
 * those prerequisites as unverified rather than treating them as passed.
 * Reuse the same Stage 8 READY prefix and preserve all existing files. */
bool tmsBringupStartInstallerPreparePlan(const char* stagingPrefix);

/* Stage 14: READ-ONLY destination inspection. Reverify the Stage 8 package,
 * validate all embedded CRCs, and inspect whether /mtd2/apupdate exists.
 * Unchecked version reports status=UNVERIFIED when the File HAL cannot
 * prove available space/mount/update flag state. It is NOT installation
 * readiness and does not open, copy or modify /mtd2/apupdate. */
bool tmsBringupStartDestinationInspect(const char* stagingPrefix);

/* Checked variant: application supplies authoritative READ-ONLY platform
 * filesystem/update-state evidence. Fails closed (-116) when any required
 * evidence is absent or the destination already exists. Even on PASS,
 * publisher authentication and atomic replacement remain unresolved. */
bool tmsBringupStartDestinationChecked(
    const char* stagingPrefix, PnaApupdateDestinationProbe probe,
    void* probeContext);

/* Stage 14B: READ ONLY. Inspect existing /mtd2/apupdate after rehashing the
 * managed source and validating its embedded files. Classify the destination
 * header and compare unchanged/rewritten content with verified staging.
 * Does NOT query pending-update status (SDK API unavailable), does NOT modify
 * any files, and NEVER installs, sets flags or reboots. Code=140 is an
 * inspection result, NOT approval to replace an existing updater artifact. */
bool tmsBringupStartExistingDestinationInspect(const char* stagingPrefix);

/* Stage 14C: read-only diagnostic of destination header independent of the
 * physical file size. When a valid binary header is found, verify every
 * embedded file CRC and classify all trailing bytes (00 / FF / other).
 * Does not modify /mtd2/apupdate, staged files, journal, or update flags.
 * Reports code=141 even when the destination format is UNKNOWN; inspection
 * success does NOT mean the existing update is valid or replaceable. */
bool tmsBringupStartExistingDestinationExtended(const char* stagingPrefix);

/* Stage 14D: direct, offline, read-only 64-byte signature probe of the
 * protected /mtd2/apupdate file; no staging prefix or package rehash needed.
 * Completion code 142 indicates inspection only, NOT replacement permission. */
bool tmsBringupStartDestinationMagic(void);

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
