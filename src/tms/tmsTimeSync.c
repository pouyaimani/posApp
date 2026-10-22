#include "tmsTimeSync.h"
#include "logger.h"

#include <string.h>

/* -----------------------------------------------------------
 * Debug helpers
 * ----------------------------------------------------------- */

/**
 * @brief Return a readable name for a time-sync state.
 *
 * Avoid logging only enum numbers: numeric values can change when the
 * enum is updated, while state names remain useful during debugging.
 */
static const char* timeSyncStateName(TmsTimeSyncState state) {
    switch (state) {
    case TMS_TIME_SYNC_IDLE:
        return "IDLE";
    case TMS_TIME_SYNC_REQUESTING:
        return "REQUESTING";
    case TMS_TIME_SYNC_COMPLETED:
        return "COMPLETED";
    case TMS_TIME_SYNC_CANCELLED:
        return "CANCELLED";
    case TMS_TIME_SYNC_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

/** @brief Return a readable name for a time-sync result. */
static const char* timeSyncResultName(TmsTimeSyncResult result) {
    switch (result) {
    case TMS_TIME_SYNC_RESULT_OK:
        return "OK";
    case TMS_TIME_SYNC_RESULT_ARGUMENT:
        return "ARGUMENT";
    case TMS_TIME_SYNC_RESULT_BUSY:
        return "BUSY";
    case TMS_TIME_SYNC_RESULT_HTTP:
        return "HTTP";
    case TMS_TIME_SYNC_RESULT_SERVER:
        return "SERVER";
    case TMS_TIME_SYNC_RESULT_CANCELLED:
        return "CANCELLED";
    default:
        return "UNKNOWN";
    }
}

/* -----------------------------------------------------------
 * Completion
 * ----------------------------------------------------------- */

/**
 * @brief Finalize a time-sync operation and notify its owner.
 *
 * The HTTP API is released before the owner callback so the callback may
 * immediately start another operation on the same time-sync instance.
 * Never access timeSync after invoking the owner callback: it may have
 * released or reinitialized the object.
 */
static void finish(TmsTimeSync* timeSync, TmsTimeSyncState state,
                   TmsTimeSyncResult result) {
    TmsTimeSyncDoneCallback callback;
    void*                   userData;

    if (timeSync == NULL) {
        LOG_ERROR("TMS TIME finish(): timeSync is NULL");
        return;
    }

    LOG_TRACE("TMS TIME finish(): state %s(%d) -> %s(%d), result=%s(%d), "
              "serverCode=%d",
              timeSyncStateName(timeSync->state), (int)timeSync->state,
              timeSyncStateName(state), (int)state, timeSyncResultName(result),
              (int)result, timeSync->serverCode);

    timeSync->state  = state;
    timeSync->result = result;

    callback = timeSync->onDone;
    userData = timeSync->userData;

    LOG_TRACE("TMS TIME finish(): releasing API client before callback, "
              "callbackSet=%d",
              (int)(callback != NULL));

    tmsApiClientRelease(&timeSync->api);

    if (callback != NULL) {
        LOG_TRACE(
            "TMS TIME finish(): invoking completion callback, result=%s(%d)",
            timeSyncResultName(result), (int)result);
        callback(timeSync, result, userData);
        /* Do not access timeSync after this callback. */
    } else {
        LOG_TRACE("TMS TIME finish(): no completion callback registered");
    }
}

/* -----------------------------------------------------------
 * HTTP completion
 * ----------------------------------------------------------- */

/**
 * @brief Handle the completed server time request.
 *
 * Only callbacks belonging to an active REQUESTING operation are accepted.
 * Late callbacks after cancellation or release are ignored.
 */
static void syncDone(TmsApiClient* api, bool success, int serverCode,
                     void* userData) {
    TmsTimeSync* timeSync = (TmsTimeSync*)userData;

    LOG_TRACE("TMS TIME syncDone(): success=%d serverCode=%d apiSet=%d",
              (int)success, serverCode, (int)(api != NULL));

    if (timeSync == NULL) {
        LOG_ERROR("TMS TIME syncDone(): timeSync context is NULL");
        return;
    }

    LOG_TRACE("TMS TIME syncDone(): current state=%s(%d)",
              timeSyncStateName(timeSync->state), (int)timeSync->state);

    if (api != &timeSync->api) {
        LOG_ERROR("TMS TIME syncDone(): unexpected API client pointer");
        /* Preserve the existing behavior; state remains the authority. */
    }

    if (timeSync->state != TMS_TIME_SYNC_REQUESTING) {
        LOG_TRACE("TMS TIME syncDone(): ignoring callback in state=%s(%d)",
                  timeSyncStateName(timeSync->state), (int)timeSync->state);
        return;
    }

    timeSync->serverCode = serverCode;

    if (!success) {
        TmsTimeSyncResult failure = serverCode < 0
                                        ? TMS_TIME_SYNC_RESULT_HTTP
                                        : TMS_TIME_SYNC_RESULT_SERVER;

        LOG_ERROR("TMS TIME syncDone(): time request failed, serverCode=%d, "
                  "mappedResult=%s(%d)",
                  serverCode, timeSyncResultName(failure), (int)failure);

        finish(timeSync, TMS_TIME_SYNC_FAILED, failure);
        return;
    }

    if (serverCode != 0) {
        LOG_ERROR(
            "TMS TIME syncDone(): success flag set with nonzero serverCode=%d",
            serverCode);
        /* Preserve existing callback semantics: success is authoritative. */
    }

    LOG_TRACE("TMS TIME syncDone(): response parsed, dateTime=%s",
              timeSync->dateTime);
    LOG_TRACE("TMS TIME syncDone(): sync completed successfully");

    finish(timeSync, TMS_TIME_SYNC_COMPLETED, TMS_TIME_SYNC_RESULT_OK);
}

/* -----------------------------------------------------------
 * Initialization
 * ----------------------------------------------------------- */

/** @brief Initialize the time-sync client and its associated HTTP API. */
void tmsTimeSyncInit(TmsTimeSync* timeSync, const char* host, uint16_t port,
                     TmsTimeSyncDoneCallback callback, void* userData) {
    LOG_TRACE(
        "TMS TIME tmsTimeSyncInit(): begin port=%u hostSet=%d callbackSet=%d",
        (unsigned int)port, (int)(host != NULL), (int)(callback != NULL));

    if (timeSync == NULL) {
        LOG_ERROR("TMS TIME tmsTimeSyncInit(): timeSync is NULL");
        return;
    }

    memset(timeSync, 0, sizeof(*timeSync));

    timeSync->state      = TMS_TIME_SYNC_IDLE;
    timeSync->result     = TMS_TIME_SYNC_RESULT_OK;
    timeSync->serverCode = -1;
    timeSync->onDone     = callback;
    timeSync->userData   = userData;

    LOG_TRACE("TMS TIME tmsTimeSyncInit(): initializing API client");
    tmsApiClientInit(&timeSync->api, host, port);

    LOG_TRACE(
        "TMS TIME tmsTimeSyncInit(): initialized state=%s(%d) serverCode=%d",
        timeSyncStateName(timeSync->state), (int)timeSync->state,
        timeSync->serverCode);

    if (callback == NULL) {
        LOG_ERROR("TMS TIME tmsTimeSyncInit(): callback is NULL; start will be "
                  "rejected");
    }
}

/* -----------------------------------------------------------
 * Start
 * ----------------------------------------------------------- */

/** @brief Start the server time-sync request. */
TmsTimeSyncResult tmsTimeSyncStart(TmsTimeSync* timeSync) {
    HttpFlowResult httpResult;

    LOG_TRACE("TMS TIME tmsTimeSyncStart(): requested");

    if (timeSync == NULL) {
        LOG_ERROR("TMS TIME tmsTimeSyncStart(): timeSync is NULL");
        return TMS_TIME_SYNC_RESULT_ARGUMENT;
    }

    LOG_TRACE(
        "TMS TIME tmsTimeSyncStart(): current state=%s(%d) callbackSet=%d",
        timeSyncStateName(timeSync->state), (int)timeSync->state,
        (int)(timeSync->onDone != NULL));

    if (timeSync->onDone == NULL) {
        LOG_ERROR("TMS TIME tmsTimeSyncStart(): callback is NULL");
        return TMS_TIME_SYNC_RESULT_ARGUMENT;
    }

    if (timeSync->state != TMS_TIME_SYNC_IDLE) {
        if (!tmsTimeSyncIsFinished(timeSync)) {
            LOG_ERROR("TMS TIME tmsTimeSyncStart(): busy in state=%s(%d)",
                      timeSyncStateName(timeSync->state), (int)timeSync->state);
            return TMS_TIME_SYNC_RESULT_BUSY;
        }

        LOG_TRACE("TMS TIME tmsTimeSyncStart(): resetting finished state "
                  "%s(%d) -> IDLE",
                  timeSyncStateName(timeSync->state), (int)timeSync->state);
        timeSync->state = TMS_TIME_SYNC_IDLE;
    }

    timeSync->dateTime[0] = '\0';
    timeSync->serverCode  = -1;
    timeSync->result      = TMS_TIME_SYNC_RESULT_OK;
    timeSync->state       = TMS_TIME_SYNC_REQUESTING;

    LOG_TRACE(
        "TMS TIME tmsTimeSyncStart(): state=REQUESTING, outputCapacity=%lu",
        (unsigned long)sizeof(timeSync->dateTime));
    LOG_TRACE("TMS TIME tmsTimeSyncStart(): calling tmsApiSyncTimeStart()");

    httpResult =
        tmsApiSyncTimeStart(&timeSync->api, timeSync->dateTime,
                            sizeof(timeSync->dateTime), syncDone, timeSync);

    /* A completion callback may have run synchronously during this call.
     * Do not dereference timeSync on the HTTP_FLOW_OK path. */
    if (httpResult != HTTP_FLOW_OK) {
        LOG_ERROR(
            "TMS TIME tmsTimeSyncStart(): HTTP request start failed result=%d",
            (int)httpResult);

        finish(timeSync, TMS_TIME_SYNC_FAILED, TMS_TIME_SYNC_RESULT_HTTP);
        return TMS_TIME_SYNC_RESULT_HTTP;
    }

    LOG_TRACE("TMS TIME tmsTimeSyncStart(): HTTP request accepted");
    return TMS_TIME_SYNC_RESULT_OK;
}

/* -----------------------------------------------------------
 * Cancellation
 * ----------------------------------------------------------- */

/** @brief Cancel an in-progress time-sync request. */
void tmsTimeSyncCancel(TmsTimeSync* timeSync) {
    LOG_TRACE("TMS TIME tmsTimeSyncCancel(): requested");

    if (timeSync == NULL) {
        LOG_ERROR("TMS TIME tmsTimeSyncCancel(): timeSync is NULL");
        return;
    }

    if (timeSync->state != TMS_TIME_SYNC_REQUESTING) {
        LOG_TRACE("TMS TIME tmsTimeSyncCancel(): ignored, state=%s(%d)",
                  timeSyncStateName(timeSync->state), (int)timeSync->state);
        return;
    }

    LOG_TRACE("TMS TIME tmsTimeSyncCancel(): marking CANCELLED before HTTP "
              "cancellation");

    /* Reject any unexpected synchronous completion triggered by cancel. */
    timeSync->state = TMS_TIME_SYNC_CANCELLED;
    tmsApiClientCancel(&timeSync->api);

    LOG_TRACE("TMS TIME tmsTimeSyncCancel(): HTTP cancel returned");
    finish(timeSync, TMS_TIME_SYNC_CANCELLED, TMS_TIME_SYNC_RESULT_CANCELLED);
}

/* -----------------------------------------------------------
 * Release
 * ----------------------------------------------------------- */

/** @brief Stop any active request and reset local time-sync state. */
void tmsTimeSyncRelease(TmsTimeSync* timeSync) {
    LOG_TRACE("TMS TIME tmsTimeSyncRelease(): requested");

    if (timeSync == NULL) {
        LOG_ERROR("TMS TIME tmsTimeSyncRelease(): timeSync is NULL");
        return;
    }

    LOG_TRACE("TMS TIME tmsTimeSyncRelease(): current state=%s(%d)",
              timeSyncStateName(timeSync->state), (int)timeSync->state);

    if (timeSync->state == TMS_TIME_SYNC_REQUESTING) {
        LOG_TRACE("TMS TIME tmsTimeSyncRelease(): active request; cancelling "
                  "without notifying owner");

        /* Ignore any synchronous callback generated by cancellation. */
        timeSync->state = TMS_TIME_SYNC_IDLE;
        tmsApiClientCancel(&timeSync->api);
    }

    LOG_TRACE("TMS TIME tmsTimeSyncRelease(): releasing API client");
    tmsApiClientRelease(&timeSync->api);

    timeSync->state       = TMS_TIME_SYNC_IDLE;
    timeSync->result      = TMS_TIME_SYNC_RESULT_OK;
    timeSync->serverCode  = -1;
    timeSync->dateTime[0] = '\0';

    LOG_TRACE(
        "TMS TIME tmsTimeSyncRelease(): complete state=IDLE serverCode=-1");
}

/* -----------------------------------------------------------
 * State queries (intentionally not traced: may be polled often)
 * ----------------------------------------------------------- */

bool tmsTimeSyncIsRunning(const TmsTimeSync* timeSync) {
    return timeSync != NULL && timeSync->state == TMS_TIME_SYNC_REQUESTING;
}

bool tmsTimeSyncIsFinished(const TmsTimeSync* timeSync) {
    return timeSync != NULL && (timeSync->state == TMS_TIME_SYNC_COMPLETED ||
                                timeSync->state == TMS_TIME_SYNC_CANCELLED ||
                                timeSync->state == TMS_TIME_SYNC_FAILED);
}

const char* tmsTimeSyncGetDateTime(const TmsTimeSync* timeSync) {
    if (timeSync == NULL || timeSync->state != TMS_TIME_SYNC_COMPLETED) {
        return NULL;
    }

    return timeSync->dateTime;
}
