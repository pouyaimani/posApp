#include "tmsApiClient.h"

#include <string.h>

#define TMS_PATH_CHECK  "/api/device/upgrade/check"
#define TMS_PATH_DEVICE "/api/device/info/report"
#define TMS_PATH_RESULT "/api/device/upgrade/result/report"
#define TMS_PATH_TIME   "/api/ntp/sync"

typedef enum {
    API_RESPONSE_UPGRADE,
    API_RESPONSE_CODE,
    API_RESPONSE_TIME
} ApiResponseKind;

static void finish(TmsApiClient* api, bool success, int code) {
    TmsApiDoneCallback callback = api->onDone;
    void*              userData = api->userData;
    if (callback != NULL)
        callback(api, success, code, userData);
}

static void responseDone(TmsHttpClient* http, HttpFlowResult result,
                         const HttpResponse* response, const uint8_t* body,
                         size_t bodyLength, void* userData) {
    TmsApiClient* api    = (TmsApiClient*)userData;
    int           code   = -1;
    bool          parsed = false;
    (void)http;

    if (api == NULL || result != HTTP_FLOW_OK || response == NULL ||
        response->statusCode < 200 || response->statusCode >= 300 ||
        body == NULL) {
        if (api != NULL)
            finish(api, false, code);
        return;
    }

    switch ((ApiResponseKind)api->responseKind) {
    case API_RESPONSE_UPGRADE:
        parsed =
            tmsJsonParseUpgradeResponse(body, bodyLength, api->upgradeInfo);
        if (parsed)
            code = api->upgradeInfo->responseCode;
        break;
    case API_RESPONSE_TIME:
        parsed = tmsJsonParseTimeResponse(body, bodyLength, api->timeOutput,
                                          api->timeOutputCapacity);
        code   = parsed ? 0 : -1;
        break;
    case API_RESPONSE_CODE:
        parsed = tmsJsonParseCodeResponse(body, bodyLength, &code);
        break;
    }
    finish(api, parsed && code == 0, code);
}

static HttpFlowResult post(TmsApiClient* api, const char* path,
                           ApiResponseKind kind, TmsApiDoneCallback callback,
                           void* userData) {
    api->responseKind = (int)kind;
    api->onDone       = callback;
    api->userData     = userData;
    return tmsHttpClientPostJson(&api->http, path, api->json, responseDone,
                                 api);
}

void tmsApiClientInit(TmsApiClient* client, const char* host, uint16_t port) {
    if (client == NULL)
        return;
    memset(client, 0, sizeof(*client));
    tmsHttpClientInit(&client->http, host, port);
}

HttpFlowResult
tmsApiCheckUpgradeStart(TmsApiClient* client, const char* deviceSn,
                        const char* brandCode, const char* currentVersion,
                        int packResourceType, UpgradeInfo* info,
                        TmsApiDoneCallback callback, void* userData) {
    if (client == NULL || info == NULL || callback == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;
    memset(info, 0, sizeof(*info));
    info->responseCode = -1;
    if (!tmsJsonBuildUpgradeCheck(client->json, sizeof(client->json), deviceSn,
                                  brandCode, currentVersion, packResourceType))
        return HTTP_FLOW_ERR_REQUEST;
    client->upgradeInfo = info;
    return post(client, TMS_PATH_CHECK, API_RESPONSE_UPGRADE, callback,
                userData);
}

HttpFlowResult tmsApiReportDeviceStart(TmsApiClient*        client,
                                       const TmsDeviceInfo* info,
                                       TmsApiDoneCallback   callback,
                                       void*                userData) {
    if (client == NULL || info == NULL || callback == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;
    if (!tmsJsonBuildDeviceInfo(client->json, sizeof(client->json), info))
        return HTTP_FLOW_ERR_REQUEST;
    return post(client, TMS_PATH_DEVICE, API_RESPONSE_CODE, callback, userData);
}

HttpFlowResult tmsApiReportUpgradeStart(TmsApiClient*           client,
                                        const TmsUpgradeReport* report,
                                        TmsApiDoneCallback      callback,
                                        void*                   userData) {
    if (client == NULL || report == NULL || callback == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;
    if (!tmsJsonBuildUpgradeReport(client->json, sizeof(client->json), report))
        return HTTP_FLOW_ERR_REQUEST;
    return post(client, TMS_PATH_RESULT, API_RESPONSE_CODE, callback, userData);
}

HttpFlowResult tmsApiSyncTimeStart(TmsApiClient* client, char* dateTime,
                                   size_t capacity, TmsApiDoneCallback callback,
                                   void* userData) {
    if (client == NULL || dateTime == NULL || capacity == 0u ||
        callback == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;
    client->timeOutput         = dateTime;
    client->timeOutputCapacity = capacity;
    client->responseKind       = (int)API_RESPONSE_TIME;
    client->onDone             = callback;
    client->userData           = userData;
    return tmsHttpClientGet(&client->http, TMS_PATH_TIME, responseDone, client);
}

void tmsApiClientCancel(TmsApiClient* client) {
    if (client != NULL)
        tmsHttpClientCancel(&client->http);
}

void tmsApiClientRelease(TmsApiClient* client) {
    if (client != NULL)
        tmsHttpClientRelease(&client->http);
}
