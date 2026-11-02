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

/* Invoke repeatedly from the application event loop AFTER the existing NTH
 * tick. */
void tmsBringupProcess(void);

#endif
