#ifndef TMS_TIME_SYNC_H
#define TMS_TIME_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "tmsApiClient.h"

#define TMS_TIME_SYNC_DATE_TIME_CAPACITY 64u

typedef enum {
    TMS_TIME_SYNC_IDLE = 0,
    TMS_TIME_SYNC_REQUESTING,
    TMS_TIME_SYNC_COMPLETED,
    TMS_TIME_SYNC_CANCELLED,
    TMS_TIME_SYNC_FAILED
} TmsTimeSyncState;

typedef enum {
    TMS_TIME_SYNC_RESULT_OK = 0,
    TMS_TIME_SYNC_RESULT_CANCELLED,
    TMS_TIME_SYNC_RESULT_HTTP,
    TMS_TIME_SYNC_RESULT_SERVER,
    TMS_TIME_SYNC_RESULT_BUSY,
    TMS_TIME_SYNC_RESULT_ARGUMENT
} TmsTimeSyncResult;

typedef struct TmsTimeSync TmsTimeSync;

typedef void (*TmsTimeSyncDoneCallback)(TmsTimeSync*      timeSync,
                                        TmsTimeSyncResult result,
                                        void*             userData);

struct TmsTimeSync {
    TmsApiClient api;

    char dateTime[TMS_TIME_SYNC_DATE_TIME_CAPACITY];

    TmsTimeSyncState  state;
    TmsTimeSyncResult result;
    int               serverCode;

    TmsTimeSyncDoneCallback onDone;
    void*                   userData;
};

void tmsTimeSyncInit(TmsTimeSync* timeSync, const char* host, uint16_t port,
                     TmsTimeSyncDoneCallback callback, void* userData);

TmsTimeSyncResult tmsTimeSyncStart(TmsTimeSync* timeSync);

void tmsTimeSyncCancel(TmsTimeSync* timeSync);
void tmsTimeSyncRelease(TmsTimeSync* timeSync);

bool tmsTimeSyncIsRunning(const TmsTimeSync* timeSync);
bool tmsTimeSyncIsFinished(const TmsTimeSync* timeSync);

const char* tmsTimeSyncGetDateTime(const TmsTimeSync* timeSync);

#endif /* TMS_TIME_SYNC_H */
