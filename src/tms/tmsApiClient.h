#ifndef TMS_API_CLIENT_H
#define TMS_API_CLIENT_H

#include "tmsHttpClient.h"
#include "tmsJson.h"
#include "tmsUpgradeInfo.h"

#define TMS_API_JSON_CAPACITY 2048u

typedef void (*TmsApiDoneCallback)(struct TmsApiClient* client, bool success,
                                   int serverCode, void* userData);

typedef struct {
    TmsHttpClient      http;
    char               json[TMS_API_JSON_CAPACITY];
    UpgradeInfo*       upgradeInfo;
    char*              timeOutput;
    size_t             timeOutputCapacity;
    TmsApiDoneCallback onDone;
    void*              userData;
    int                responseKind;
} TmsApiClient;

void tmsApiClientInit(TmsApiClient* client, const char* host, uint16_t port);
HttpFlowResult
tmsApiCheckUpgradeStart(TmsApiClient* client, const char* deviceSn,
                        const char* brandCode, const char* currentVersion,
                        int packResourceType, UpgradeInfo* info,
                        TmsApiDoneCallback callback, void* userData);
HttpFlowResult tmsApiReportDeviceStart(TmsApiClient*        client,
                                       const TmsDeviceInfo* info,
                                       TmsApiDoneCallback   callback,
                                       void*                userData);
HttpFlowResult tmsApiReportUpgradeStart(TmsApiClient*           client,
                                        const TmsUpgradeReport* report,
                                        TmsApiDoneCallback      callback,
                                        void*                   userData);
HttpFlowResult tmsApiSyncTimeStart(TmsApiClient* client, char* dateTime,
                                   size_t capacity, TmsApiDoneCallback callback,
                                   void* userData);
void           tmsApiClientCancel(TmsApiClient* client);
void           tmsApiClientRelease(TmsApiClient* client);

#endif
