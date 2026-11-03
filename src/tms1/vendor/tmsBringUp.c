#include "tmsBringup.h"

#include <stdio.h>
#include <string.h>

#include "../tms.h"
#include "pnaTms.h"
#include "logger.h"
#include "common.h" /* Project MEM_ALLOC / MEM_FREE implementation. */

#if !PNA_TMS_ENABLE_DIAGNOSTICS
#error "tmsBringup.c must be compiled ONLY with -DPNA_TMS_ENABLE_DIAGNOSTICS=1"
#endif

/* Keep the backend alive through NTH callbacks and manager completion.
 * Never free it from onDone: processPna() may still be returning from the
 * callback and access its context. Free only outside the backend stack. */
static PnaTms*          bringupBackend;
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
        else if (currentCheckpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE)
            LOG_TRACE("TMS BRINGUP: FIRST SECTOR FILE PASS "
                      "(4096 bytes read back; scratch deleted; no install)");
        else if (currentCheckpoint == PNA_TMS_CHECKPOINT_FULL_FILE)
            LOG_TRACE("TMS BRINGUP: FULL FILE PASS "
                      "(scratch retained for inspection; no install)");
        else if (currentCheckpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5)
            LOG_TRACE("TMS BRINGUP: INCREMENTAL MD5 PASS "
                      "(readback CRC32 + manifest MD5 match; no install)");
        else if (currentCheckpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5)
            LOG_TRACE("TMS BRINGUP: ALL FILES MD5 PASS "
                      "(every file verified and retained; no install)");
        else if (currentCheckpoint == PNA_TMS_CHECKPOINT_MANAGED_READY)
            LOG_TRACE("TMS BRINGUP: STAGE8 READY_TO_INSTALL "
                      "(journal saved; manifest MD5 verified; no install; "
                      "publisher authentication still required)");
    } else {
        LOG_TRACE(
            "TMS BRINGUP: CHECKPOINT NOT PASSED result=%d error=%d code=%ld",
            (int)status->result, (int)status->error, (long)status->backendCode);
    }
}

/* Safe only when no manager process()/backend callback is on the stack. */
static void freeIdleBackend(void) {
    if (bringupBackend && !tmsMgr()->isBusy()) {
        MEM_FREE(bringupBackend);
        bringupBackend = NULL;
    }
}

static bool startDiagnostic(const char* host, uint16_t port, bool useTls,
                            const PnaTmsIdentity*   identity,
                            PnaTmsCheckpoint        checkpoint,
                            const char*             scratchPrefix,
                            PnaTmsStorageSpaceCheck spaceCheck,
                            void*                   spaceCheckContext) {
    if (useTls) {
        LOG_TRACE("TMS BRINGUP: TLS requested; unsupported by current NTH");
        return false;
    }
    if (!host || !host[0] || !port || tmsMgr()->isBusy() ||
        ((checkpoint == PNA_TMS_CHECKPOINT_MANIFEST ||
          checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR ||
          checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE ||
          checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE ||
          checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 ||
          checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
          checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY) &&
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
    cfg.port                = port;
    cfg.tls                 = useTls;
    cfg.checkpoint          = checkpoint;
    cfg.checkStorageSpace   = spaceCheck;
    cfg.storageSpaceContext = spaceCheckContext;
    if (checkpoint == PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE ||
        checkpoint == PNA_TMS_CHECKPOINT_FULL_FILE ||
        checkpoint == PNA_TMS_CHECKPOINT_INCREMENTAL_MD5 ||
        checkpoint == PNA_TMS_CHECKPOINT_ALL_FILES_MD5 ||
        checkpoint == PNA_TMS_CHECKPOINT_MANAGED_READY) {
        if (!scratchPrefix || !scratchPrefix[0]) {
            LOG_TRACE("TMS BRINGUP: scratchPrefix required");
            return false;
        }
        n = snprintf(cfg.stagingPrefix, sizeof(cfg.stagingPrefix), "%s",
                     scratchPrefix);
        if (n <= 0 || (size_t)n >= sizeof(cfg.stagingPrefix)) {
            LOG_TRACE("TMS BRINGUP: scratchPrefix too long");
            return false;
        }
    }
    if (identity)
        cfg.identity = *identity;

    freeIdleBackend();
    bringupBackend = (PnaTms*)MEM_ALLOC(sizeof(*bringupBackend));
    if (!bringupBackend) {
        LOG_TRACE("TMS BRINGUP: MEM_ALLOC failed size=%lu",
                  (unsigned long)sizeof(PnaTms));
        return false;
    }
    /* pnaTmsInit checks p->active before initializing its fields. */
    memset(bringupBackend, 0, sizeof(*bringupBackend));
    if (!pnaTmsInit(bringupBackend, &cfg)) {
        LOG_TRACE("TMS BRINGUP: PNA initialization failed");
        MEM_FREE(bringupBackend);
        bringupBackend = NULL;
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
    bool accepted = tmsMgr()->run(&runCfg, TMS_TRIGGER_MANUAL);
    /* The backend may complete synchronously while run() is executing. */
    if (!tmsMgr()->isBusy())
        freeIdleBackend();
    return accepted;
}

bool tmsBringupStartConnectOnly(const char* host, uint16_t port, bool useTls) {
    return startDiagnostic(host, port, useTls, NULL,
                           PNA_TMS_CHECKPOINT_CONNECTED, NULL, NULL, NULL);
}

bool tmsBringupStartManifestOnly(const char* host, uint16_t port, bool useTls,
                                 const PnaTmsIdentity* identity) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_MANIFEST, NULL, NULL, NULL);
}

bool tmsBringupStartFirstSector(const char* host, uint16_t port, bool useTls,
                                const PnaTmsIdentity* identity) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_FIRST_SECTOR, NULL, NULL, NULL);
}

bool tmsBringupStartFirstSectorWrite(const char* host, uint16_t port,
                                     bool                  useTls,
                                     const PnaTmsIdentity* identity,
                                     const char*           scratchPrefix) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_FIRST_SECTOR_WRITE, scratchPrefix,
                           NULL, NULL);
}

bool tmsBringupStartFullFile(const char* host, uint16_t port, bool useTls,
                             const PnaTmsIdentity* identity,
                             const char*           scratchPrefix) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_FULL_FILE, scratchPrefix, NULL,
                           NULL);
}

bool tmsBringupStartIncrementalMd5(const char* host, uint16_t port, bool useTls,
                                   const PnaTmsIdentity* identity,
                                   const char*           scratchPrefix) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_INCREMENTAL_MD5, scratchPrefix,
                           NULL, NULL);
}

bool tmsBringupStartAllFilesMd5(const char* host, uint16_t port, bool useTls,
                                const PnaTmsIdentity* identity,
                                const char*           scratchPrefix) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_ALL_FILES_MD5, scratchPrefix,
                           NULL, NULL);
}

bool tmsBringupStartAllFilesMd5Checked(const char* host, uint16_t port,
                                       bool                    useTls,
                                       const PnaTmsIdentity*   identity,
                                       const char*             scratchPrefix,
                                       PnaTmsStorageSpaceCheck checkSpace,
                                       void*                   checkContext) {
    if (!checkSpace)
        return false;
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_ALL_FILES_MD5, scratchPrefix,
                           checkSpace, checkContext);
}

bool tmsBringupStartManagedReady(const char* host, uint16_t port, bool useTls,
                                 const PnaTmsIdentity* identity,
                                 const char*           stagingPrefix) {
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_MANAGED_READY, stagingPrefix,
                           NULL, NULL);
}

bool tmsBringupStartManagedReadyChecked(const char* host, uint16_t port,
                                        bool                    useTls,
                                        const PnaTmsIdentity*   identity,
                                        const char*             stagingPrefix,
                                        PnaTmsStorageSpaceCheck checkSpace,
                                        void*                   checkContext) {
    if (!checkSpace)
        return false;
    return startDiagnostic(host, port, useTls, identity,
                           PNA_TMS_CHECKPOINT_MANAGED_READY, stagingPrefix,
                           checkSpace, checkContext);
}

PnaTmsStageStatus tmsBringupStage8Inspect(const char*      stagingPrefix,
                                          PnaTmsStageInfo* info) {
    if (tmsMgr()->isBusy())
        return PNA_TMS_STAGE_IO_ERROR;
    return pnaTmsStageInspect(stagingPrefix, info);
}

bool tmsBringupStage8Discard(const char* stagingPrefix) {
    if (tmsMgr()->isBusy())
        return false;
    return pnaTmsStageDiscard(stagingPrefix, true);
}

void tmsBringupProcess(void) {
    /* NTH already ticked once from global event loop. */
    TmsMgr* mgr = tmsMgr();
    if (mgr->isBusy() && !pumpLogged) {
        pumpLogged = true;
        LOG_TRACE("TMS BRINGUP: process loop is running");
    }
    mgr->process();
    /* Completion notifications can occur inside process(). Delay heap free
     * until that stack has fully returned. */
    if (!mgr->isBusy()) {
        freeIdleBackend();
        pumpLogged = false;
    }
}
