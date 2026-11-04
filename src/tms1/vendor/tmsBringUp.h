#ifndef TMS_BRINGUP_H_
#define TMS_BRINGUP_H_

#include <stdbool.h>
#include <stdint.h>
#include "pnaTms.h"

/* Compact production staging: no installer, activation or diagnostic stages.
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
void tmsBringupProcess(void);

#endif
