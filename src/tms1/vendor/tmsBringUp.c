#include "tmsBringup.h"

#include <stdio.h>
#include <string.h>
#include "common.h"
#include "logger.h"
#include "../tms.h"

/* Owned across manager/NTH callbacks; never freed from onDone(). */
static PnaTms* backend;

static void onDone(const TmsStatus* status, void* context) {
    (void)context;
    if (status)
        LOG_TRACE("TMS: done result=%d err=%d code=%ld", (int)status->result,
                  (int)status->error, (long)status->backendCode);
}

static void freeIdleBackend(void) {
    if (backend && !tmsMgr()->isBusy()) {
        MEM_FREE(backend);
        backend = NULL;
    }
}

bool tmsBringupStartManagedReadyChecked(const char* host, uint16_t port,
                                        bool                    useTls,
                                        const PnaTmsIdentity*   identity,
                                        const char*             stagingPrefix,
                                        PnaTmsStorageSpaceCheck checkSpace,
                                        void*                   checkContext) {
    if (!host || !identity || !stagingPrefix || useTls || !port ||
        tmsMgr()->isBusy())
        return false;
    freeIdleBackend();
    if (backend)
        return false;

    PnaTmsConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    int h = snprintf(cfg.host, sizeof(cfg.host), "%s", host);
    int p = snprintf(cfg.stagingPrefix, sizeof(cfg.stagingPrefix), "%s",
                     stagingPrefix);
    if (h <= 0 || (size_t)h >= sizeof(cfg.host) || p <= 0 ||
        (size_t)p >= sizeof(cfg.stagingPrefix))
        return false;
    cfg.port                = port;
    cfg.identity            = *identity;
    cfg.checkStorageSpace   = checkSpace;
    cfg.storageSpaceContext = checkContext;

    backend = (PnaTms*)MEM_ALLOC(sizeof(*backend));
    if (!backend)
        return false;
    memset(backend, 0, sizeof(*backend));
    if (!pnaTmsInit(backend, &cfg)) {
        MEM_FREE(backend);
        backend = NULL;
        return false;
    }
    const TmsCfg runCfg = {.backend = pnaTmsBackend(backend), .onDone = onDone};
    bool         accepted = tmsMgr()->run(&runCfg, TMS_TRIGGER_MANUAL);
    freeIdleBackend();
    return accepted;
}

bool tmsBringupStartManagedReady(const char* host, uint16_t port, bool useTls,
                                 const PnaTmsIdentity* identity,
                                 const char*           stagingPrefix) {
    return tmsBringupStartManagedReadyChecked(host, port, useTls, identity,
                                              stagingPrefix, NULL, NULL);
}

PnaTmsStageStatus tmsBringupStage8Inspect(const char*      prefix,
                                          PnaTmsStageInfo* info) {
    return pnaTmsStageInspect(prefix, info);
}

bool tmsBringupStage8Discard(const char* prefix) {
    if (tmsMgr()->isBusy())
        return false;
    return pnaTmsStageDiscard(prefix, true);
}

void tmsBringupProcess(void) {
    tmsMgr()->process();
    freeIdleBackend();
}
