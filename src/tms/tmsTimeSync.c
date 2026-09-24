#include "tmsTimeSync.h"

#include <string.h>

static void finish(TmsTimeSync* timeSync, TmsTimeSyncState state,
                   TmsTimeSyncResult result) {
    TmsTimeSyncDoneCallback callback;
    void*                   userData;

    if (timeSync == NULL)
        return;

    timeSync->state  = state;
    timeSync->result = result;

    callback = timeSync->onDone;
    userData = timeSync->userData;

    /*
     * Release before notifying the owner. This allows the callback to start
     * another time-sync operation on the same object without the completed
     * request subsequently releasing the new operation.
     */
    tmsApiClientRelease(&timeSync->api);

    if (callback != NULL)
        callback(timeSync, result, userData);
}

static void syncDone(TmsApiClient* api, bool success, int serverCode,
                     void* userData) {
    TmsTimeSync* timeSync = (TmsTimeSync*)userData;

    (void)api;

    if (timeSync == NULL || timeSync->state != TMS_TIME_SYNC_REQUESTING) {
        return;
    }

    timeSync->serverCode = serverCode;

    if (!success) {
        finish(timeSync, TMS_TIME_SYNC_FAILED,
               serverCode < 0 ? TMS_TIME_SYNC_RESULT_HTTP
                              : TMS_TIME_SYNC_RESULT_SERVER);
        return;
    }

    finish(timeSync, TMS_TIME_SYNC_COMPLETED, TMS_TIME_SYNC_RESULT_OK);
}

void tmsTimeSyncInit(TmsTimeSync* timeSync, const char* host, uint16_t port,
                     TmsTimeSyncDoneCallback callback, void* userData) {
    if (timeSync == NULL)
        return;

    memset(timeSync, 0, sizeof(*timeSync));

    timeSync->state      = TMS_TIME_SYNC_IDLE;
    timeSync->result     = TMS_TIME_SYNC_RESULT_OK;
    timeSync->serverCode = -1;
    timeSync->onDone     = callback;
    timeSync->userData   = userData;

    tmsApiClientInit(&timeSync->api, host, port);
}

TmsTimeSyncResult tmsTimeSyncStart(TmsTimeSync* timeSync) {
    HttpFlowResult httpResult;

    if (timeSync == NULL || timeSync->onDone == NULL)
        return TMS_TIME_SYNC_RESULT_ARGUMENT;

    if (timeSync->state != TMS_TIME_SYNC_IDLE) {
        if (!tmsTimeSyncIsFinished(timeSync))
            return TMS_TIME_SYNC_RESULT_BUSY;

        timeSync->state = TMS_TIME_SYNC_IDLE;
    }

    timeSync->dateTime[0] = '\0';
    timeSync->serverCode  = -1;
    timeSync->result      = TMS_TIME_SYNC_RESULT_OK;
    timeSync->state       = TMS_TIME_SYNC_REQUESTING;

    httpResult =
        tmsApiSyncTimeStart(&timeSync->api, timeSync->dateTime,
                            sizeof(timeSync->dateTime), syncDone, timeSync);

    if (httpResult != HTTP_FLOW_OK) {
        finish(timeSync, TMS_TIME_SYNC_FAILED, TMS_TIME_SYNC_RESULT_HTTP);
        return TMS_TIME_SYNC_RESULT_HTTP;
    }

    return TMS_TIME_SYNC_RESULT_OK;
}

void tmsTimeSyncCancel(TmsTimeSync* timeSync) {
    if (timeSync == NULL || timeSync->state != TMS_TIME_SYNC_REQUESTING) {
        return;
    }

    tmsApiClientCancel(&timeSync->api);

    finish(timeSync, TMS_TIME_SYNC_CANCELLED, TMS_TIME_SYNC_RESULT_CANCELLED);
}

void tmsTimeSyncRelease(TmsTimeSync* timeSync) {
    if (timeSync == NULL)
        return;

    if (timeSync->state == TMS_TIME_SYNC_REQUESTING)
        tmsApiClientCancel(&timeSync->api);

    tmsApiClientRelease(&timeSync->api);

    timeSync->state       = TMS_TIME_SYNC_IDLE;
    timeSync->result      = TMS_TIME_SYNC_RESULT_OK;
    timeSync->serverCode  = -1;
    timeSync->dateTime[0] = '\0';
}

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
