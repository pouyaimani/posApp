#ifndef TMS_BRINGUP_H_
#define TMS_BRINGUP_H_

#include <stdbool.h>
#include <stdint.h>

#include "pnaTmsProtocol.h"

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

/* Stage 6: first file only; same download procedure, 1KB of readback
 * work per tmsBringupProcess() call. Compare stored CRC32 to received
 * CRC32 and stored MD5 to the raw 16-byte server manifest MD5.
 * Retains the verified scratch on PASS; removes it on verification failure.
 * NEVER installs or activates a package. Use a NEW scratch prefix.
 */
bool tmsBringupStartIncrementalMd5(const char* host, uint16_t port,
                                   bool useTls,
                                   const PnaTmsIdentity* identity,
                                   const char* scratchPrefix);

/* Invoke repeatedly from the application event loop AFTER the existing NTH tick. */
void tmsBringupProcess(void);

#endif
