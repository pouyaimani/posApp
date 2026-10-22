
#include "tmsApiClient.h"
#include "logger.h"
#include "error.h"

#include <string.h>

#define TMS_PATH_CHECK  "/api/device/upgrade/check"
#define TMS_PATH_DEVICE "/api/device/info/report"
#define TMS_PATH_RESULT "/api/device/upgrade/result/report"
#define TMS_PATH_TIME   "/api/ntp/sync"

/* -----------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------- */

/**
 * @brief Calculate string length without reading beyond capacity.
 *
 * @return String length, or capacity if no null terminator exists.
 */
static size_t boundedStringLength(const char* str, size_t capacity) {
    size_t length = 0;

    if (str == NULL)
        return 0;

    while (length < capacity && str[length] != '\0')
        length++;

    return length;
}

/**
 * @brief Invoke the API completion callback.
 *
 * The callback may release or reuse the client.
 * Therefore, do not access api after invoking the callback.
 */
static void finish(TmsApiClient* api, bool success, int code) {
    if (api == NULL) {
        LOG_ERROR("TMS finish(): api is NULL");
        return;
    }

    TmsApiDoneCallback callback = api->onDone;
    void*              userData = api->userData;

    LOG_TRACE("TMS finish(): kind=%d success=%d code=%d callbackSet=%d",
              api->responseKind, (int)success, code, (int)(callback != NULL));

    if (callback == NULL) {
        LOG_ERROR("TMS finish(): completion callback is NULL");
        return;
    }

    LOG_TRACE("TMS finish(): invoking completion callback");

    callback(api, success, code, userData);

    /* Do not access api here: callback may have released it. */
}

/* -----------------------------------------------------------
 * HTTP Response
 * ----------------------------------------------------------- */

/**
 * @brief Handle the completed HTTP request and parse its response.
 *
 * Handles:
 * - Upgrade check response
 * - Time synchronization response
 * - Generic response code
 *
 * HTTP errors and malformed responses are reported through
 * the API completion callback.
 */
static void responseDone(TmsHttpClient* http, HttpFlowResult result,
                         const HttpResponse* response, const uint8_t* body,
                         size_t bodyLength, void* userData) {
    TmsApiClient* api = (TmsApiClient*)userData;

    int  code   = -1;
    bool parsed = false;

    int httpStatus = response != NULL ? (int)response->statusCode : -1;

    (void)http;

    LOG_TRACE("TMS responseDone(): result=%d httpStatus=%d bodyLen=%lu",
              (int)result, httpStatus, (unsigned long)bodyLength);

    /* Validate API context. */
    if (api == NULL) {
        LOG_ERROR("TMS responseDone(): api context is NULL");
        return;
    }

    LOG_TRACE("TMS responseDone(): responseKind=%d", api->responseKind);

    /* Check HTTP flow result. */
    if (result != HTTP_FLOW_OK) {
        LOG_ERROR("TMS responseDone(): HTTP flow failed result=%d status=%d",
                  (int)result, httpStatus);

        finish(api, false, code);
        return;
    }

    /* Validate HTTP response pointer. */
    if (response == NULL) {
        LOG_ERROR("TMS responseDone(): HTTP response is NULL");

        finish(api, false, code);
        return;
    }

    /* Check HTTP status code. */
    if (httpStatus < 200 || httpStatus >= 300) {
        LOG_ERROR("TMS responseDone(): HTTP status error=%d", httpStatus);

        finish(api, false, code);
        return;
    }

    LOG_TRACE("TMS responseDone(): HTTP status accepted=%d", httpStatus);

    /* Validate response body. */
    if (body == NULL || bodyLength == 0u) {
        LOG_ERROR("TMS responseDone(): empty response body ptrSet=%d len=%lu",
                  (int)(body != NULL), (unsigned long)bodyLength);

        finish(api, false, code);
        return;
    }

    /* Parse the response based on the API operation. */
    switch ((ApiResponseKind)api->responseKind) {

    case API_RESPONSE_UPGRADE:

        LOG_TRACE("TMS responseDone(): parsing upgrade response");

        if (api->upgradeInfo == NULL) {
            LOG_ERROR("TMS responseDone(): upgradeInfo is NULL");
            break;
        }

        parsed =
            tmsJsonParseUpgradeResponse(body, bodyLength, api->upgradeInfo);

        if (parsed) {
            code = api->upgradeInfo->responseCode;

            LOG_TRACE("TMS responseDone(): upgrade parsed responseCode=%d",
                      code);
        } else {
            LOG_ERROR("TMS responseDone(): failed to parse upgrade JSON");
        }

        break;

    case API_RESPONSE_TIME:

        LOG_TRACE("TMS responseDone(): parsing time response");

        if (api->timeOutput == NULL || api->timeOutputCapacity == 0u) {

            LOG_ERROR(
                "TMS responseDone(): invalid time output buffer capacity=%lu",
                (unsigned long)api->timeOutputCapacity);

            break;
        }

        parsed = tmsJsonParseTimeResponse(body, bodyLength, api->timeOutput,
                                          api->timeOutputCapacity);

        code = parsed ? 0 : -1;

        if (parsed) {
            LOG_TRACE("TMS responseDone(): time parsed successfully");
        } else {
            LOG_ERROR("TMS responseDone(): failed to parse time JSON");
        }

        break;

    case API_RESPONSE_CODE:

        LOG_TRACE("TMS responseDone(): parsing generic code response");

        parsed = tmsJsonParseCodeResponse(body, bodyLength, &code);

        if (parsed) {
            LOG_TRACE("TMS responseDone(): generic responseCode=%d", code);
        } else {
            LOG_ERROR("TMS responseDone(): failed to parse response code");
        }

        break;

    default:

        LOG_ERROR("TMS responseDone(): unknown responseKind=%d",
                  api->responseKind);

        break;
    }

    LOG_TRACE("TMS responseDone(): completed parsed=%d code=%d", (int)parsed,
              code);

    /*
     * Preserve existing behavior:
     * success indicates successful parsing, not necessarily
     * a successful server business response.
     */
    finish(api, parsed, code);
}

/* -----------------------------------------------------------
 * POST Helper
 * ----------------------------------------------------------- */

/**
 * @brief Configure and start a JSON POST request.
 *
 * Logs the request path, operation type, payload length,
 * and the immediate HTTP start result.
 */
static HttpFlowResult post(TmsApiClient* api, const char* path,
                           ApiResponseKind kind, TmsApiDoneCallback callback,
                           void* userData) {
    HttpFlowResult result;
    size_t         jsonLength;

    LOG_TRACE("TMS post(): starting kind=%d", (int)kind);

    if (api == NULL || path == NULL || callback == NULL) {
        LOG_ERROR("TMS post(): invalid args api=%d path=%d callback=%d",
                  (int)(api != NULL), (int)(path != NULL),
                  (int)(callback != NULL));

        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    jsonLength = boundedStringLength(api->json, sizeof(api->json));

    if (jsonLength >= sizeof(api->json)) {
        LOG_ERROR("TMS post(): JSON is not terminated capacity=%lu",
                  (unsigned long)sizeof(api->json));

        return HTTP_FLOW_ERR_REQUEST;
    }

    LOG_TRACE("TMS post(): path=%s kind=%d jsonLen=%lu", path, (int)kind,
              (unsigned long)jsonLength);

    api->responseKind = (int)kind;
    api->onDone       = callback;
    api->userData     = userData;

    LOG_TRACE("TMS post(): calling tmsHttpClientPostJson");

    result =
        tmsHttpClientPostJson(&api->http, path, api->json, responseDone, api);

    if (result != HTTP_FLOW_OK) {
        LOG_ERROR("TMS post(): HTTP start failed path=%s result=%d", path,
                  (int)result);
    } else {
        LOG_TRACE("TMS post(): HTTP request started path=%s", path);
    }

    return result;
}

/* -----------------------------------------------------------
 * Initialization
 * ----------------------------------------------------------- */

/**
 * @brief Initialize TMS API client.
 */
void tmsApiClientInit(TmsApiClient* client, const char* host, uint16_t port) {
    LOG_TRACE("TMS tmsApiClientInit(): port=%u hostSet=%d", (unsigned int)port,
              (int)(host != NULL));

    if (client == NULL) {
        LOG_ERROR("TMS tmsApiClientInit(): client is NULL");
        return;
    }

    memset(client, 0, sizeof(*client));

    LOG_TRACE("TMS tmsApiClientInit(): client memory cleared");

    tmsHttpClientInit(&client->http, host, port);

    LOG_TRACE("TMS tmsApiClientInit(): HTTP client initialized");
}

/* -----------------------------------------------------------
 * Check Upgrade
 * ----------------------------------------------------------- */

/**
 * @brief Build and start the device upgrade check request.
 */
HttpFlowResult
tmsApiCheckUpgradeStart(TmsApiClient* client, const char* deviceSn,
                        const char* brandCode, const char* currentVersion,
                        int packResourceType, UpgradeInfo* info,
                        TmsApiDoneCallback callback, void* userData) {
    LOG_TRACE("TMS tmsApiCheckUpgradeStart(): begin resourceType=%d",
              packResourceType);

    if (client == NULL || info == NULL || callback == NULL) {
        LOG_ERROR("TMS tmsApiCheckUpgradeStart(): invalid args client=%d "
                  "info=%d callback=%d",
                  (int)(client != NULL), (int)(info != NULL),
                  (int)(callback != NULL));

        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    LOG_TRACE(
        "TMS tmsApiCheckUpgradeStart(): serialSet=%d brandSet=%d versionSet=%d",
        (int)(deviceSn != NULL), (int)(brandCode != NULL),
        (int)(currentVersion != NULL));

    memset(info, 0, sizeof(*info));
    info->responseCode = -1;

    LOG_TRACE("TMS tmsApiCheckUpgradeStart(): building upgrade JSON");

    if (!tmsJsonBuildUpgradeCheck(client->json, sizeof(client->json), deviceSn,
                                  brandCode, currentVersion,
                                  packResourceType)) {

        LOG_ERROR("TMS tmsApiCheckUpgradeStart(): JSON build failed");

        return HTTP_FLOW_ERR_REQUEST;
    }

    LOG_TRACE("TMS tmsApiCheckUpgradeStart(): JSON build successful");

    client->upgradeInfo = info;

    return post(client, TMS_PATH_CHECK, API_RESPONSE_UPGRADE, callback,
                userData);
}

/* -----------------------------------------------------------
 * Report Device Information
 * ----------------------------------------------------------- */

/**
 * @brief Build and start the device information report.
 */
HttpFlowResult tmsApiReportDeviceStart(TmsApiClient*        client,
                                       const TmsDeviceInfo* info,
                                       TmsApiDoneCallback   callback,
                                       void*                userData) {
    LOG_TRACE("TMS tmsApiReportDeviceStart(): begin");

    if (client == NULL || info == NULL || callback == NULL) {
        LOG_ERROR("TMS tmsApiReportDeviceStart(): invalid args client=%d "
                  "info=%d callback=%d",
                  (int)(client != NULL), (int)(info != NULL),
                  (int)(callback != NULL));

        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    LOG_TRACE("TMS tmsApiReportDeviceStart(): building device info JSON");

    if (!tmsJsonBuildDeviceInfo(client->json, sizeof(client->json), info)) {

        LOG_ERROR("TMS tmsApiReportDeviceStart(): JSON build failed");

        return HTTP_FLOW_ERR_REQUEST;
    }

    LOG_TRACE("TMS tmsApiReportDeviceStart(): JSON build successful");

    return post(client, TMS_PATH_DEVICE, API_RESPONSE_CODE, callback, userData);
}

/* -----------------------------------------------------------
 * Report Upgrade Result
 * ----------------------------------------------------------- */

/**
 * @brief Build and start the upgrade result report.
 */
HttpFlowResult tmsApiReportUpgradeStart(TmsApiClient*           client,
                                        const TmsUpgradeReport* report,
                                        TmsApiDoneCallback      callback,
                                        void*                   userData) {
    LOG_TRACE("TMS tmsApiReportUpgradeStart(): begin");

    if (client == NULL || report == NULL || callback == NULL) {
        LOG_ERROR("TMS tmsApiReportUpgradeStart(): invalid args client=%d "
                  "report=%d callback=%d",
                  (int)(client != NULL), (int)(report != NULL),
                  (int)(callback != NULL));

        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    LOG_TRACE("TMS tmsApiReportUpgradeStart(): building upgrade report JSON");

    if (!tmsJsonBuildUpgradeReport(client->json, sizeof(client->json),
                                   report)) {

        LOG_ERROR("TMS tmsApiReportUpgradeStart(): JSON build failed");

        return HTTP_FLOW_ERR_REQUEST;
    }

    LOG_TRACE("TMS tmsApiReportUpgradeStart(): JSON build successful");

    return post(client, TMS_PATH_RESULT, API_RESPONSE_CODE, callback, userData);
}

/* -----------------------------------------------------------
 * Time Synchronization
 * ----------------------------------------------------------- */

/**
 * @brief Start the server time synchronization request.
 */
HttpFlowResult tmsApiSyncTimeStart(TmsApiClient* client, char* dateTime,
                                   size_t capacity, TmsApiDoneCallback callback,
                                   void* userData) {
    HttpFlowResult result;

    LOG_TRACE("TMS tmsApiSyncTimeStart(): begin capacity=%lu",
              (unsigned long)capacity);

    if (client == NULL || dateTime == NULL || capacity == 0u ||
        callback == NULL) {

        LOG_ERROR("TMS tmsApiSyncTimeStart(): invalid args client=%d output=%d "
                  "capacity=%lu callback=%d",
                  (int)(client != NULL), (int)(dateTime != NULL),
                  (unsigned long)capacity, (int)(callback != NULL));

        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    client->timeOutput         = dateTime;
    client->timeOutputCapacity = capacity;
    client->responseKind       = (int)API_RESPONSE_TIME;
    client->onDone             = callback;
    client->userData           = userData;

    LOG_TRACE("TMS tmsApiSyncTimeStart(): GET path=%s", TMS_PATH_TIME);

    result =
        tmsHttpClientGet(&client->http, TMS_PATH_TIME, responseDone, client);

    if (result != HTTP_FLOW_OK) {
        LOG_ERROR("TMS tmsApiSyncTimeStart(): HTTP GET start failed result=%d",
                  (int)result);
    } else {
        LOG_TRACE("TMS tmsApiSyncTimeStart(): HTTP GET started successfully");
    }

    return result;
}

/* -----------------------------------------------------------
 * Cancel
 * ----------------------------------------------------------- */

/**
 * @brief Cancel the active TMS HTTP operation.
 */
void tmsApiClientCancel(TmsApiClient* client) {
    LOG_TRACE("TMS tmsApiClientCancel(): requested");

    if (client == NULL) {
        LOG_ERROR("TMS tmsApiClientCancel(): client is NULL");
        return;
    }

    LOG_TRACE("TMS tmsApiClientCancel(): cancelling HTTP client");

    tmsHttpClientCancel(&client->http);
}

/* -----------------------------------------------------------
 * Release
 * ----------------------------------------------------------- */

/**
 * @brief Release TMS HTTP client resources.
 */
void tmsApiClientRelease(TmsApiClient* client) {
    LOG_TRACE("TMS tmsApiClientRelease(): requested");

    if (client == NULL) {
        LOG_ERROR("TMS tmsApiClientRelease(): client is NULL");
        return;
    }

    LOG_TRACE("TMS tmsApiClientRelease(): releasing HTTP client");

    tmsHttpClientRelease(&client->http);
}
