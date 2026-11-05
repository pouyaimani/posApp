#include "tmsBringup.h"

#include <stdio.h>
#include <string.h>
#include "common.h"
#include "logger.h"
#include "sys/sys.h"
#include "../tms.h"

/* Owned across manager/NTH callbacks; never freed from onDone(). */
static PnaTms* backend;

#if TMS_ENABLE_APUPDATE_WRITER
/* Deferred until tmsBringupProcess() returns from the manager/backend pump.
 * Never reboot directly from a backend notification callback. */
static bool updateActivationPending;

/* A destination-preparation success is distinct from READY staging (8),
 * preflight (9), scratch-copy (10) and audit (11) successes. */
#if TMS_DEV_POS_BUILD && TMS_DEBUG_SKIP_SIGNATURE_FOR_PREPARE
#define TMS_PREPARED_CODE_FOR_BUILD                                            \
    13 /* Unsigned DEV POS: NEVER production                                   \
        */
#else
#define TMS_PREPARED_CODE_FOR_BUILD                                            \
    12 /* SDK-signed destination preparation                                   \
        */
#endif
#endif

static void onDone(const TmsStatus* status, void* context) {
    (void)context;
    if (!status) {
        LOG_TRACE("TMS: null completion status");
        return;
    }

    LOG_TRACE("TMS: done op=%llu result=%d err=%d code=%ld",
              (unsigned long long)status->operationId, (int)status->result,
              (int)status->error, (long)status->backendCode);

#if TMS_ENABLE_APUPDATE_WRITER
    /* Only a fully copied and verified updater destination may be activated.
     * The development result 13 skips SDK signature validation intentionally.
     */
    if (status->result == TMS_RESULT_SUCCESS && status->error == TMS_ERR_NONE &&
        status->backendCode == TMS_PREPARED_CODE_FOR_BUILD) {
        updateActivationPending = true;
        LOG_TRACE("TMS: APUPDATE prepared; activation queued op=%llu",
                  (unsigned long long)status->operationId);
    }
#endif
}
static void freeIdleBackend(void) {
    if (backend && !tmsMgr()->isBusy()) {
        MEM_FREE(backend);
        backend = NULL;
    }
}

static bool startBackend(const PnaTmsConfig* cfg) {
#if TMS_ENABLE_APUPDATE_WRITER
    if (updateActivationPending) {
        LOG_TRACE("TMS: cannot start another operation before activation");
        return false;
    }
#endif
    if (tmsMgr()->isBusy())
        return false;
    freeIdleBackend();
    if (backend)
        return false;
    backend = (PnaTms*)MEM_ALLOC(sizeof(*backend));
    if (!backend)
        return false;
    memset(backend, 0, sizeof(*backend));
    if (!pnaTmsInit(backend, cfg)) {
        MEM_FREE(backend);
        backend = NULL;
        return false;
    }
    const TmsCfg runCfg = {.backend = pnaTmsBackend(backend), .onDone = onDone};
    bool         accepted = tmsMgr()->run(&runCfg, TMS_TRIGGER_MANUAL);
    freeIdleBackend();
    return accepted;
}

bool tmsBringupStartManagedReadyChecked(const char* host, uint16_t port,
                                        bool                    useTls,
                                        const PnaTmsIdentity*   identity,
                                        const char*             stagingPrefix,
                                        PnaTmsStorageSpaceCheck checkSpace,
                                        void*                   checkContext) {
    if (!host || !identity || !stagingPrefix || useTls || !port)
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
    return startBackend(&cfg);
}

/* Offline, read-only verification. Does not inspect or touch /mtd2. */
bool tmsBringupStartInstallerPreflight(const char* stagingPrefix) {
    if (!stagingPrefix)
        return false;
    PnaTmsConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    int n = snprintf(cfg.stagingPrefix, sizeof(cfg.stagingPrefix), "%s",
                     stagingPrefix);
    if (n <= 0 || (size_t)n >= sizeof(cfg.stagingPrefix))
        return false;
    cfg.prepareOnly = true;
    return startBackend(&cfg);
}

#if TMS_ENABLE_APUPDATE_AUDIT
bool tmsBringupStartApupdateAudit(const char* stagingPrefix) {
    if (!stagingPrefix)
        return false;
    PnaTmsConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    int n = snprintf(cfg.stagingPrefix, sizeof(cfg.stagingPrefix), "%s",
                     stagingPrefix);
    if (n <= 0 || (size_t)n >= sizeof(cfg.stagingPrefix))
        return false;
    cfg.prepareOnly   = true;
    cfg.auditApupdate = true;
    return startBackend(&cfg);
}
#endif

#if TMS_ENABLE_APUPDATE_WRITER
/* The caller supplies an explicit one-operation replacement decision.
 * SDK signature verification remains mandatory inside the PNA backend. */
bool tmsBringupStartApupdatePrepare(const char*            prefix,
                                    PnaTmsAuthorizeReplace authorize,
                                    void*                  context) {
    if (!prefix || !authorize)
        return false;
    PnaTmsConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    int n =
        snprintf(cfg.stagingPrefix, sizeof(cfg.stagingPrefix), "%s", prefix);
    if (n <= 0 || (size_t)n >= sizeof(cfg.stagingPrefix))
        return false;
    cfg.prepareOnly          = true;
    cfg.prepareDestination   = true;
    cfg.authorizeReplace     = authorize;
    cfg.authorizationContext = context;
    return startBackend(&cfg);
}
#endif

#if TMS_ENABLE_COPY_TO_SCRATCH
/* Offline read-only preflight followed by a reversible COPY to a disposable
 * /mtd0 scratch file. Does not touch the updater partition. */
bool tmsBringupStartScratchCopy(const char* stagingPrefix) {
    if (!stagingPrefix)
        return false;
    PnaTmsConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    int n = snprintf(cfg.stagingPrefix, sizeof(cfg.stagingPrefix), "%s",
                     stagingPrefix);
    if (n <= 0 || (size_t)n >= sizeof(cfg.stagingPrefix))
        return false;
    cfg.prepareOnly   = true;
    cfg.copyToScratch = true;
    return startBackend(&cfg);
}
#endif

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

#if TMS_ENABLE_APUPDATE_WRITER
    if (!updateActivationPending)
        return;

    /* No activation while an operation is active or its backend is allocated.
     * This is deliberately outside onDone(), after file/NTH cleanup. */
    if (tmsMgr()->isBusy() || backend) {
        LOG_TRACE("TMS: activation deferred; backend still busy");
        return;
    }

    updateActivationPending = false; /* One attempt only. */
    System* system          = sys();
    if (!system || !system->vtable.setUpdateFlag || !system->vtable.reboot) {
        LOG_TRACE("TMS: activation failed; System HAL methods unavailable");
        return;
    }

    /* DEV integration: setUpdateFlag() and restart() are assumed to take no
     * arguments. The System HAL declaration was not supplied, so check its
     * return-value contract before relying on this for a production build. */
    LOG_TRACE("TMS: APUPDATE ready; requesting update flag");
    OOP_CALL(system, setUpdateFlag);
    LOG_TRACE("TMS: update flag requested; restarting");
    OOP_CALL(system, reboot);

    /* Expected only if the device HAL's restart call returns. */
    LOG_TRACE("TMS: restart returned unexpectedly");
#endif
}

#if defined(TMS_ENABLE_HEADER_FIRST_PROBE) && TMS_ENABLE_HEADER_FIRST_PROBE
/* One-off disposable File HAL test. Compile out of the shipping firmware.
 * Never use the protected /mtd2/apupdate destination for this operation. */
bool tmsBringupProbeHeaderFirst(void) {
    static const char    path[]      = "/mtd0/tms_hd_01.bin";
    static const uint8_t header[16]  = {0x48, 0x44, 0x52, 0x2d, 0x31, 0x35,
                                        0x43, 0x2d, 0x30, 0x31, 0x2d, 0x54,
                                        0x45, 0x53, 0x54, 0x21};
    static const uint8_t payload[16] = {0xa5, 0x5a, 0x01, 0xfe, 0x12, 0xed,
                                        0x34, 0xcb, 0x56, 0xa9, 0x78, 0x87,
                                        0x9a, 0x65, 0xbc, 0x43};
    uint8_t              check[16];
    FileHandle*          handle     = NULL;
    bool                 created    = false;
    bool                 passed     = false;
    unsigned             phase      = 0u;
    long                 actualSize = -1L;

    /* Refuse collisions. T3Rtos open("wb+") does not truncate an existing file.
     */
    if (tmsMgr()->isBusy() || OOP_CALL(file(), exists, path)) {
        LOG_TRACE("TMS HDR-FIRST: refused busy/file-exists");
        return false;
    }
    phase = 1u;
    LOG_TRACE("TMS HDR-FIRST: open disposable file");
    handle = OOP_CALL(file(), open, path, "wb+");
    if (!handle)
        goto cleanup;
    created = true;

    /* overwrite() maps to sdkFileWrite(), which starts at offset zero. */
    phase = 2u;
    LOG_TRACE("TMS HDR-FIRST: overwrite header");
    if (OOP_CALL(file(), overwrite, header, 1u, sizeof(header), handle) !=
        sizeof(header))
        goto cleanup;

    /* HAL overwrite() sets handle->pos to the header length. */
    phase = 3u;
    LOG_TRACE("TMS HDR-FIRST: append payload at offset 16");
    if (OOP_CALL(file(), tell, handle) != (long)sizeof(header) ||
        OOP_CALL(file(), write, payload, 1u, sizeof(payload), handle) !=
            sizeof(payload))
        goto cleanup;
    phase = 4u;
    if (OOP_CALL(file(), flush, handle) != 0 ||
        OOP_CALL(file(), sync, handle) != 0)
        goto cleanup;
    if (OOP_CALL(file(), close, handle) != 0) {
        handle = NULL;
        goto cleanup;
    }
    handle = NULL;

    phase = 5u;
    LOG_TRACE("TMS HDR-FIRST: reopen and verify");
    handle = OOP_CALL(file(), open, path, "rb");
    if (!handle)
        goto cleanup;
    actualSize = OOP_CALL(file(), size, handle);
    if (actualSize != (long)(sizeof(header) + sizeof(payload)))
        goto cleanup;
    if (OOP_CALL(file(), read, check, 1u, sizeof(header), handle) !=
            sizeof(header) ||
        memcmp(check, header, sizeof(header)) != 0)
        goto cleanup;
    if (OOP_CALL(file(), read, check, 1u, sizeof(payload), handle) !=
            sizeof(payload) ||
        memcmp(check, payload, sizeof(payload)) != 0)
        goto cleanup;
    passed = true;

cleanup:
    if (handle && OOP_CALL(file(), close, handle) != 0)
        passed = false;
    /* Delete ONLY the disposable file created by this test. */
    if (created) {
        if (OOP_CALL(file(), remove, path) != 0 ||
            OOP_CALL(file(), exists, path))
            passed = false;
    }
    LOG_TRACE("TMS HDR-FIRST: pass=%d phase=%u size=%ld removed=%d",
              (int)passed, phase, actualSize,
              (int)(created && !OOP_CALL(file(), exists, path)));
    return passed;
}
#endif /* TMS_ENABLE_HEADER_FIRST_PROBE */
