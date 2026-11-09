#include "tms.h"
#include "logger.h"

#include <limits.h>
#include <string.h>

static TmsMgr __tmsMgr;

static bool isActiveState(TmsState state) {
    switch (state) {
    case TMS_STATE_STARTING:
    case TMS_STATE_CONNECTING:
    case TMS_STATE_CHECKING:
    case TMS_STATE_DOWNLOADING:
    case TMS_STATE_VERIFYING:
    case TMS_STATE_INSTALLING:
        return true;
    default:
        return false;
    }
}

static bool isFinalResult(TmsResult result) {
    return result == TMS_RESULT_SUCCESS || result == TMS_RESULT_NO_UPDATE ||
           result == TMS_RESULT_FAILURE || result == TMS_RESULT_CANCELED ||
           result == TMS_RESULT_DIAGNOSTIC;
}

/*
 * Return a rejection only to the new request's callback. Never modify the
 * singleton: it may already be processing another vendor operation.
 */
static bool rejectRun(const TmsCfg* cfg, TmsTrigger trigger, TmsError err) {
    LOG_TRACE("TmsMgr: run rejected trigger=%d error=%d", (int)trigger,
              (int)err);

    if (cfg && cfg->onDone) {
        const TmsStatus rejected = {
            .operationId = 0,
            .trigger     = trigger,
            .result      = TMS_RESULT_REJECT,
            .state       = TMS_STATE_IDLE,
            .error       = err,
        };
        cfg->onDone(&rejected, cfg->userData);
    }
    return false;
}

/*
 * Deliver terminal status exactly once for an accepted operation.
 * IMPORTANT: status is committed, active is cleared, and callbacks are
 * captured before onDone: the callback is allowed to call run() again.
 */
static void finish(uint64_t operationId, TmsResult result, TmsError error,
                   int32_t backendCode) {
    TmsMgr* mgr = &__tmsMgr;

    if (!mgr->active || mgr->status.operationId != operationId ||
        !isFinalResult(result)) {
        LOG_TRACE("TmsMgr: ignored finish op=%llu current=%llu active=%d",
                  (unsigned long long)operationId,
                  (unsigned long long)mgr->status.operationId,
                  (int)mgr->active);
        return;
    }

    mgr->status.result = result;
    mgr->status.error =
        (result == TMS_RESULT_FAILURE)
            ? (error == TMS_ERR_NONE ? TMS_ERR_BACKEND_FAILURE : error)
            : TMS_ERR_NONE;
    mgr->status.backendCode     = backendCode;
    mgr->status.cancelRequested = false;

    switch (result) {
    case TMS_RESULT_SUCCESS:
    case TMS_RESULT_NO_UPDATE:
        mgr->status.state    = TMS_STATE_COMPLETED;
        mgr->status.progress = 100;
        break;
    case TMS_RESULT_DIAGNOSTIC:
        /* A connection checkpoint is not an installed update. */
        mgr->status.state = TMS_STATE_COMPLETED;
        break;
    case TMS_RESULT_CANCELED:
        mgr->status.state = TMS_STATE_CANCELED;
        break;
    case TMS_RESULT_FAILURE:
    default:
        mgr->status.state = TMS_STATE_FAILED;
        break;
    }

    /* Snapshot, not a pointer to singleton storage (can change in onDone). */
    TmsStatus   status   = mgr->status;
    TmsStatusCb callback = mgr->config.onDone;
    void*       userData = mgr->config.userData;
    mgr->active          = false;

    LOG_TRACE("TmsMgr: done op=%llu result=%d error=%d backend=%ld progress=%u",
              (unsigned long long)operationId, (int)result, (int)status.error,
              (long)backendCode, (unsigned)status.progress);

    if (callback)
        callback(&status, userData);

    /* Do not touch mgr here: callback may have begun another operation. */
}

/*
 * Common notification sink for all vendor backends.
 * Every event is tagged with its operationId to filter stale callbacks.
 * A backend is required to dispatch notifications on the manager's thread.
 */
static void backendNotify(void* notifyContext, uint64_t operationId,
                          const TmsBackendEvent* event) {
    TmsMgr* mgr = &__tmsMgr;
    if (notifyContext != mgr || !event) {
        LOG_TRACE("TmsMgr: ignored notification: bad context/event");
        return;
    }

    if (!mgr->active || mgr->status.operationId != operationId) {
        LOG_TRACE("TmsMgr: stale event ignored op=%llu current=%llu active=%d",
                  (unsigned long long)operationId,
                  (unsigned long long)mgr->status.operationId,
                  (int)mgr->active);
        return;
    }

    if (event->type == TMS_BACKEND_EVENT_DONE) {
        if (!isFinalResult(event->result)) {
            LOG_TRACE("TmsMgr: invalid terminal result=%d", (int)event->result);
            finish(operationId, TMS_RESULT_FAILURE, TMS_ERR_INVALID_EVENT,
                   event->backendCode);
            return;
        }
        finish(operationId, event->result, event->error, event->backendCode);
        return;
    }

    if (event->type != TMS_BACKEND_EVENT_PROGRESS ||
        !isActiveState(event->state) || event->progress > 100 ||
        (event->fileCount && event->fileIndex > event->fileCount)) {
        LOG_TRACE("TmsMgr: ignoring invalid progress event type=%d stage=%d "
                  "progress=%u file=%u/%u",
                  (int)event->type, (int)event->state,
                  (unsigned)event->progress, (unsigned)event->fileIndex,
                  (unsigned)event->fileCount);
        return;
    }

    mgr->status.state       = event->state;
    mgr->status.progress    = event->progress;
    mgr->status.fileIndex   = event->fileIndex;
    mgr->status.fileCount   = event->fileCount;
    mgr->status.backendCode = event->backendCode;

    LOG_TRACE("TmsMgr: progress op=%llu state=%d progress=%u file=%u/%u",
              (unsigned long long)operationId, (int)event->state,
              (unsigned)event->progress, (unsigned)event->fileIndex,
              (unsigned)event->fileCount);

    TmsStatus   snapshot = mgr->status;
    TmsStatusCb callback = mgr->config.onProgress;
    void*       userData = mgr->config.userData;
    if (callback)
        callback(&snapshot, userData);
}

/*
 * Start exactly one operation. A backend may notify completion inline from
 * start(), and that completion callback may immediately begin another run.
 * The operationId guard prevents old fallback handling from failing it.
 */
static bool run(const TmsCfg* cfg, TmsTrigger trigger) {
    TmsMgr* mgr = &__tmsMgr;
    LOG_TRACE("TmsMgr: run requested trigger=%d", (int)trigger);

    if (!cfg)
        return rejectRun(cfg, trigger, TMS_ERR_NULL_CONFIG);
    if (mgr->active)
        return rejectRun(cfg, trigger, TMS_ERR_BUSY);
    if (!cfg->backend.ops || !cfg->backend.ops->start)
        return rejectRun(cfg, trigger, TMS_ERR_INVALID_BACKEND);
    if ((cfg->backend.flags & TMS_BACKEND_FLAG_BLOCKING_START) &&
        !cfg->allowBlocking)
        return rejectRun(cfg, trigger, TMS_ERR_BLOCKING_NOT_ALLOWED);
    if (mgr->lastOperationId == UINT64_MAX)
        return rejectRun(cfg, trigger, TMS_ERR_OPERATION_ID_EXHAUSTED);
    if (trigger != TMS_TRIGGER_MANUAL && trigger != TMS_TRIGGER_AUTO &&
        trigger != TMS_TRIGGER_SCHEDULED)
        return rejectRun(cfg, trigger, TMS_ERR_INVALID_EVENT);

    /* All validation passed. Snapshot configuration before invoking backend. */
    mgr->config = *cfg;
    ++mgr->lastOperationId;
    const uint64_t startedId = mgr->lastOperationId;
    mgr->status              = (TmsStatus){
        .operationId = startedId,
        .trigger     = trigger,
        .result      = TMS_RESULT_RUNNING,
        .state       = TMS_STATE_STARTING,
        .error       = TMS_ERR_NONE,
        .progress    = 0,
    };
    mgr->active = true;

    /* Callback can run and even start another operation before return. */
    const TmsBackend backend = mgr->config.backend;
    LOG_TRACE("TmsMgr: accepted op=%llu blocking=%d",
              (unsigned long long)startedId,
              (int)((backend.flags & TMS_BACKEND_FLAG_BLOCKING_START) != 0));

    bool started = backend.ops->start(backend.context, trigger, startedId,
                                      backendNotify, mgr);

    if (!started && mgr->active && mgr->status.operationId == startedId) {
        LOG_TRACE("TmsMgr: backend returned false without completion op=%llu",
                  (unsigned long long)startedId);
        finish(startedId, TMS_RESULT_FAILURE, TMS_ERR_BACKEND_START, 0);
    }

    /* Accepted != succeeded. The result is always delivered through onDone. */
    return true;
}

/* Poll-style async backends can be pumped from the app event loop. */
static void process(void) {
    TmsMgr* mgr = &__tmsMgr;
    if (!mgr->active)
        return;

    const TmsBackend backend = mgr->config.backend;
    if (backend.ops && backend.ops->process)
        backend.ops->process(backend.context);
}

/*
 * Cancellation is a request, not a forced completion. The backend must
 * notify DONE before the manager can release and accept another operation.
 */
static bool cancel(void) {
    TmsMgr* mgr = &__tmsMgr;
    if (!mgr->active) {
        LOG_TRACE("TmsMgr: cancel rejected: not active");
        return false;
    }
    if (mgr->status.cancelRequested)
        return true;

    const TmsBackend backend = mgr->config.backend;
    if (!backend.ops || !backend.ops->cancel) {
        LOG_TRACE("TmsMgr: cancellation unsupported");
        return false;
    }

    const uint64_t operationId  = mgr->status.operationId;
    mgr->status.cancelRequested = true;
    LOG_TRACE("TmsMgr: cancel requested op=%llu",
              (unsigned long long)operationId);

    /* cancel() is allowed to synchronously report terminal completion. */
    bool accepted = backend.ops->cancel(backend.context, operationId);
    if (!accepted && mgr->active && mgr->status.operationId == operationId) {
        mgr->status.cancelRequested = false;
        LOG_TRACE("TmsMgr: cancellation request declined op=%llu",
                  (unsigned long long)operationId);
    }
    return accepted;
}

static bool reset(void) {
    TmsMgr* mgr = &__tmsMgr;
    if (mgr->active) {
        LOG_TRACE("TmsMgr: reset rejected: active");
        return false;
    }

    mgr->config = (TmsCfg){0};
    mgr->status = (TmsStatus){
        .result = TMS_RESULT_NONE,
        .state  = TMS_STATE_IDLE,
        .error  = TMS_ERR_NONE,
    };
    /* lastOperationId deliberately preserved for stale-callback filtering. */
    LOG_TRACE("TmsMgr: reset complete");
    return true;
}

static bool isBusy(void) { return __tmsMgr.active; }

static TmsStatus getStatus(void) { return __tmsMgr.status; }

OOP_CTOR(TmsMgr) {
    LOG_TRACE("TmsMgr: initializing singleton");
    self->config = (TmsCfg){0};
    self->status = (TmsStatus){
        .result = TMS_RESULT_NONE,
        .state  = TMS_STATE_IDLE,
        .error  = TMS_ERR_NONE,
    };
    self->lastOperationId = 0;
    self->active          = false;

    self->run       = run;
    self->process   = process;
    self->cancel    = cancel;
    self->reset     = reset;
    self->isBusy    = isBusy;
    self->getStatus = getStatus;
}

TmsMgr* tmsMgr(void) {
    CALL_ONCE(OOP_CALL_CTOR(TmsMgr, &__tmsMgr););
    return &__tmsMgr;
}
