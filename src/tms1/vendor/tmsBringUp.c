#include "tmsBringup.h"

#include <stdio.h>
#include <string.h>

#include "../tms.h"
#include "pnaTms.h"
#include "logger.h"

#if !PNA_TMS_ENABLE_DIAGNOSTICS
#error "tmsBringup.c must be compiled ONLY with -DPNA_TMS_ENABLE_DIAGNOSTICS=1"
#endif

static PnaTms*          bringupBackend; /* Alive across all NTH callbacks. */
static bool             pumpLogged;
static PnaTmsCheckpoint currentCheckpoint;

static void onProgress(const TmsStatus* status, void* user) {
    (void)user;
    if (!status)
        return;
    LOG_TRACE("TMS BRINGUP: progress op=%llu state=%d percent=%u files=%u/%u",
              (unsigned long long)status->operationId, (int)status->state,
              (unsigned)status->progress, (unsigned)status->fileIndex,
              (unsigned)status->fileCount);
}

static void onDone(const TmsStatus* status, void* user) {
    (void)user;
    if (!status)
        return;
    LOG_TRACE("TMS BRINGUP: DONE op=%llu result=%d error=%d backend=%ld",
              (unsigned long long)status->operationId, (int)status->result,
              (int)status->error, (long)status->backendCode);

    if (status->result == TMS_RESULT_DIAGNOSTIC &&
        status->error == TMS_ERR_NONE &&
        status->backendCode == (int)currentCheckpoint) {
        if (currentCheckpoint == PNA_TMS_CHECKPOINT_CONNECTED)
            LOG_TRACE("TMS BRINGUP: CONNECT ONLY PASS (no command sent)");
        else if (currentCheckpoint == PNA_TMS_CHECKPOINT_MANIFEST)
            LOG_TRACE(
                "TMS BRINGUP: MANIFEST ONLY PASS (no download/files/install)");
        else if (currentCheckpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR)
            LOG_TRACE("TMS BRINGUP: FIRST SECTOR PASS "
                      "(validated response 21; no file writes/install)");
    } else {
        LOG_TRACE(
            "TMS BRINGUP: CHECKPOINT NOT PASSED result=%d error=%d code=%ld",
            (int)status->result, (int)status->error, (long)status->backendCode);
    }
}

static bool startDiagnostic(const char* host, uint16_t port, bool useTls,
                            const PnaTmsIdentity* identity,
                            PnaTmsCheckpoint      checkpoint) {
    if (useTls) {
        LOG_TRACE("TMS BRINGUP: TLS requested; unsupported by current NTH");
        return false;
    }
    if (!host || !host[0] || !port || tmsMgr()->isBusy() ||
        ((checkpoint == PNA_TMS_CHECKPOINT_MANIFEST ||
          checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR) &&
         !identity)) {
        LOG_TRACE("TMS BRINGUP: invalid inputs or busy");
        return false;
    }

    PnaTmsConfig cfg = {0};
    int          n   = snprintf(cfg.host, sizeof(cfg.host), "%s", host);
    if (n <= 0 || (size_t)n >= sizeof(cfg.host)) {
        LOG_TRACE("TMS BRINGUP: invalid host length");
        return false;
    }
    cfg.port       = port;
    cfg.tls        = useTls;
    cfg.checkpoint = checkpoint;
    if (identity)
        cfg.identity = *identity;

    bringupBackend = MEM_ALLOC(sizeof(*bringupBackend));

    if (!pnaTmsInit(bringupBackend, &cfg)) {
        LOG_TRACE("TMS BRINGUP: PNA initialization failed");
        return false;
    }
    TmsCfg runCfg = {
        .backend    = pnaTmsBackend(bringupBackend),
        .onProgress = onProgress,
        .onDone     = onDone,
    };
    currentCheckpoint = checkpoint;
    pumpLogged        = false;
    LOG_TRACE("TMS BRINGUP: start checkpoint=%d real PNA %s:%u",
              (int)checkpoint, cfg.host, (unsigned)cfg.port);
    return tmsMgr()->run(&runCfg, TMS_TRIGGER_MANUAL);
}

bool tmsBringupStartConnectOnly(const char* host, uint16_t port, bool useTls) {
    return startDiagnostic(host, port, useTls, NULL,
                           PNA_TMS_CHECKPOINT_CONNECTED);
}

bool tmsBringupStartManifestOnly(const char* host, uint16_t port, bool useTls,
                                 const PnaTmsIdentity* identity) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_MANIFEST);
}

bool tmsBringupStartFirstSector(const char* host, uint16_t port, bool useTls,
                                const PnaTmsIdentity* identity) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_FIRST_SECTOR);
}

void tmsBringupProcess(void) {
    /* NTH already ticked once from global event loop. */
    TmsMgr* mgr = tmsMgr();
    if (mgr->isBusy() && !pumpLogged) {
        pumpLogged = true;
        LOG_TRACE("TMS BRINGUP: process loop is running");
    }
    mgr->process();
}
