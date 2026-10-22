#include "tmsHttpClient.h"
#include "logger.h"
#include "error.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"

#include <string.h>

/* -----------------------------------------------------------
 * HTTP flow completion
 * ----------------------------------------------------------- */

/**
 * @brief Forward an HTTP flow completion to the TMS client owner.
 *
 * Completion may occur synchronously inside httpFlowStart(). Such a
 * completion is deferred until the start call returns successfully.
 *
 * The user's callback may release or reuse the client. Do not access
 * client or flow after invoking that callback.
 */
static void httpFinished(HttpFlow* flow, const HttpFlowStatus* status) {
    TmsHttpClient*      client;
    TmsHttpDoneCallback callback;
    void*               userData;
    const HttpResponse* response;
    const uint8_t*      body;
    size_t              bodyLength;
    int                 httpStatus;

    if (flow == NULL) {
        LOG_ERROR("TMS HTTP httpFinished(): flow is NULL");
        return;
    }

    if (status == NULL) {
        LOG_ERROR("TMS HTTP httpFinished(): status is NULL");
        return;
    }

    client = (TmsHttpClient*)flow->userData;
    if (client == NULL) {
        LOG_ERROR("TMS HTTP httpFinished(): flow userData is NULL, result=%d",
                  (int)status->result);
        return;
    }

    LOG_TRACE("TMS HTTP httpFinished(): result=%d state=%d busy=%d starting=%d",
              (int)status->result, (int)flow->state, (int)client->busy,
              (int)client->starting);

    if (client->starting) {
        /* The request start has not returned yet. Notify the owner later. */
        client->completionDuringStart = true;
        client->deferredStatus        = *status;

        LOG_TRACE("TMS HTTP httpFinished(): completion deferred, result=%d",
                  (int)status->result);
        return;
    }

    response   = httpFlowGetResponse(flow);
    body       = httpFlowGetBody(flow);
    bodyLength = httpFlowGetBodyLength(flow);
    httpStatus = response != NULL ? (int)response->statusCode : -1;

    callback = client->onDone;
    userData = client->userData;

    /* Mark idle before the callback, allowing the owner to start again. */
    client->busy = false;

    LOG_TRACE("TMS HTTP httpFinished(): result=%d httpStatus=%d bodyLen=%lu "
              "responseSet=%d bodySet=%d callbackSet=%d",
              (int)status->result, httpStatus, (unsigned long)bodyLength,
              (int)(response != NULL), (int)(body != NULL),
              (int)(callback != NULL));

    if (status->result != HTTP_FLOW_OK) {
        LOG_ERROR("TMS HTTP httpFinished(): HTTP flow failed, result=%d "
                  "httpStatus=%d",
                  (int)status->result, httpStatus);
    }

    if (callback == NULL) {
        LOG_TRACE("TMS HTTP httpFinished(): completion ignored (no callback)");
        return;
    }

    LOG_TRACE("TMS HTTP httpFinished(): invoking completion callback");
    callback(client, status->result, response, body, bodyLength, userData);
    /* Do not access client/flow here: the callback may release them. */
}

/* -----------------------------------------------------------
 * HTTP flow state changes
 * ----------------------------------------------------------- */

/** @brief Show HTTP connection progress and trace state transitions. */
static void httpOnState(HttpFlow* flow, const HttpFlowState state) {
    (void)flow;

    LOG_TRACE("TMS HTTP httpOnState(): state=%d", (int)state);

    switch (state) {
    case HTTP_FLOW_CONNECTING:
        LOG_TRACE("TMS HTTP httpOnState(): CONNECTING");
        SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CONNECTIING), "");
        break;

    case HTTP_FLOW_SENDING:
        LOG_TRACE("TMS HTTP httpOnState(): SENDING");
        SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_SENDING_DATA), "");
        break;

    case HTTP_FLOW_RECEIVING_HEADERS:
        LOG_TRACE("TMS HTTP httpOnState(): RECEIVING_HEADERS");
        SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_RECEIVING_DATA), "");
        break;

    case HTTP_FLOW_RECEIVING_BODY:
        LOG_TRACE("TMS HTTP httpOnState(): RECEIVING_BODY");
        SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_RECEIVING_DATA), "");
        break;

    default:
        /* Not every flow state requires a UI update. */
        LOG_TRACE("TMS HTTP httpOnState(): no UI update for state=%d",
                  (int)state);
        break;
    }
}

/* -----------------------------------------------------------
 * Initialization
 * ----------------------------------------------------------- */

/** @brief Initialize the HTTP client and its underlying flow. */
void tmsHttpClientInit(TmsHttpClient* client, const char* host, uint16_t port) {
    LOG_TRACE("TMS HTTP tmsHttpClientInit(): begin port=%u hostSet=%d",
              (unsigned int)port, (int)(host != NULL));

    if (client == NULL) {
        LOG_ERROR("TMS HTTP tmsHttpClientInit(): client is NULL");
        return;
    }

    memset(client, 0, sizeof(*client));

    if (host != NULL) {
        size_t hostCapacity = sizeof(client->host) - 1u;

        strncpy(client->host, host, hostCapacity);
        client->host[hostCapacity] = '\0';

        if (hostCapacity > 0u && client->host[hostCapacity - 1u] != '\0') {
            LOG_ERROR("TMS HTTP tmsHttpClientInit(): hostname may be truncated "
                      "max=%lu",
                      (unsigned long)hostCapacity);
        }
    } else {
        LOG_TRACE("TMS HTTP tmsHttpClientInit(): host is NULL");
    }

    client->port = port;

    LOG_TRACE("TMS HTTP tmsHttpClientInit(): initializing HTTP flow");
    httpFlowInit(&client->flow);

    LOG_TRACE("TMS HTTP tmsHttpClientInit(): initialized port=%u state=%d",
              (unsigned int)client->port, (int)client->flow.state);
}

/* -----------------------------------------------------------
 * Generic request
 * ----------------------------------------------------------- */

/**
 * @brief Start a TMS HTTP request.
 *
 * Startup errors are returned to the caller and are not also delivered
 * through the completion callback.
 */
HttpFlowResult
tmsHttpClientRequest(TmsHttpClient* client, HttpMethod method, const char* path,
                     const HttpFlowHeader* headers, size_t headerCount,
                     const uint8_t* body, size_t bodyLength,
                     TmsHttpDoneCallback callback, void* userData) {
    HttpFlowRequest   request;
    HttpFlowCallbacks callbacks;
    HttpFlowResult    result;

    LOG_TRACE("TMS HTTP tmsHttpClientRequest(): begin method=%d pathSet=%d "
              "headers=%lu bodyLen=%lu",
              (int)method, (int)(path != NULL), (unsigned long)headerCount,
              (unsigned long)bodyLength);

    if (client == NULL || path == NULL || callback == NULL) {
        LOG_ERROR("TMS HTTP tmsHttpClientRequest(): invalid args client=%d "
                  "path=%d callback=%d",
                  (int)(client != NULL), (int)(path != NULL),
                  (int)(callback != NULL));
        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    if (client->busy) {
        LOG_ERROR("TMS HTTP tmsHttpClientRequest(): busy, path=%s flowState=%d",
                  path, (int)client->flow.state);
        return HTTP_FLOW_ERR_BUSY;
    }

    if ((headerCount != 0u && headers == NULL) ||
        (bodyLength != 0u && body == NULL)) {
        LOG_ERROR("TMS HTTP tmsHttpClientRequest(): invalid buffer/count pair "
                  "headersSet=%d headerCount=%lu bodySet=%d bodyLen=%lu",
                  (int)(headers != NULL), (unsigned long)headerCount,
                  (int)(body != NULL), (unsigned long)bodyLength);
        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    LOG_TRACE("TMS HTTP tmsHttpClientRequest(): path=%s method=%d port=%u "
              "flowState=%d headerCount=%lu bodyLen=%lu",
              path, (int)method, (unsigned int)client->port,
              (int)client->flow.state, (unsigned long)headerCount,
              (unsigned long)bodyLength);

    if (client->flow.state != HTTP_FLOW_IDLE) {
        LOG_TRACE(
            "TMS HTTP tmsHttpClientRequest(): releasing previous flow state=%d",
            (int)client->flow.state);
        httpFlowRelease(&client->flow);
        LOG_TRACE("TMS HTTP tmsHttpClientRequest(): previous flow released");
    }

    memset(&request, 0, sizeof(request));
    request.method      = method;
    request.path        = path;
    request.headers     = headers;
    request.headerCount = headerCount;
    request.body        = body;
    request.bodyLength  = bodyLength;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.onDone  = httpFinished;
    callbacks.onState = httpOnState;

    client->onDone                = callback;
    client->userData              = userData;
    client->busy                  = true;
    client->starting              = true;
    client->completionDuringStart = false;

    LOG_TRACE("TMS HTTP tmsHttpClientRequest(): calling httpFlowStart path=%s",
              path);

    result = httpFlowStart(&client->flow, NULL, client->host, client->port,
                           &request, &callbacks, client);

    client->starting = false;

    LOG_TRACE(
        "TMS HTTP tmsHttpClientRequest(): httpFlowStart returned result=%d "
        "flowState=%d deferred=%d",
        (int)result, (int)client->flow.state,
        (int)client->completionDuringStart);

    if (result != HTTP_FLOW_OK) {
        /* Report synchronous startup failures only via return value. */
        client->busy                  = false;
        client->completionDuringStart = false;

        LOG_ERROR(
            "TMS HTTP tmsHttpClientRequest(): start failed path=%s result=%d",
            path, (int)result);
        return result;
    }

    if (client->completionDuringStart) {
        HttpFlowStatus deferredStatus = client->deferredStatus;

        client->completionDuringStart = false;

        LOG_TRACE(
            "TMS HTTP tmsHttpClientRequest(): delivering deferred completion "
            "result=%d",
            (int)deferredStatus.result);

        httpFinished(&client->flow, &deferredStatus);
        /* Callback may have released or reused client. */
        return result;
    }

    LOG_TRACE(
        "TMS HTTP tmsHttpClientRequest(): request active path=%s state=%d",
        path, (int)client->flow.state);

    return result;
}

/* -----------------------------------------------------------
 * JSON POST
 * ----------------------------------------------------------- */
#define TMS_HOST "sabztms.ir"
/** @brief Send a JSON POST request with standard TMS headers. */
HttpFlowResult tmsHttpClientPostJson(TmsHttpClient* client, const char* path,
                                     const char*         json,
                                     TmsHttpDoneCallback callback,
                                     void*               userData) {
    static const HttpFlowHeader headers[] = {
        {"Content-Type", "application/json"},
        {"HTTP:", TMS_HOST},
        {"Accept", "application/json"},
        {"Connection", "close"}};
    size_t jsonLength;

    LOG_TRACE("TMS HTTP tmsHttpClientPostJson(): begin pathSet=%d jsonSet=%d",
              (int)(path != NULL), (int)(json != NULL));

    if (json == NULL) {
        LOG_ERROR("TMS HTTP tmsHttpClientPostJson(): json is NULL");
        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    /* Caller must supply a null-terminated JSON string. */
    jsonLength = strlen(json);

    LOG_TRACE("TMS HTTP tmsHttpClientPostJson(): jsonLen=%lu",
              (unsigned long)jsonLength);

    return tmsHttpClientRequest(client, HTTP_METHOD_POST, path, headers,
                                sizeof(headers) / sizeof(headers[0]),
                                (const uint8_t*)json, jsonLength, callback,
                                userData);
}

/* -----------------------------------------------------------
 * GET
 * ----------------------------------------------------------- */

/** @brief Send a GET request with standard TMS headers. */
HttpFlowResult tmsHttpClientGet(TmsHttpClient* client, const char* path,
                                TmsHttpDoneCallback callback, void* userData) {
    static const HttpFlowHeader headers[] = {{"Accept", "application/json"},
                                             {"Connection", "close"}};

    LOG_TRACE("TMS HTTP tmsHttpClientGet(): begin pathSet=%d",
              (int)(path != NULL));

    return tmsHttpClientRequest(client, HTTP_METHOD_GET, path, headers,
                                sizeof(headers) / sizeof(headers[0]), NULL, 0u,
                                callback, userData);
}

/* -----------------------------------------------------------
 * Cancellation
 * ----------------------------------------------------------- */

/**
 * @brief Cancel an active request without forwarding a completion callback.
 *
 * The cancellation owner performs its own CANCELLED state transition.
 */
void tmsHttpClientCancel(TmsHttpClient* client) {
    LOG_TRACE("TMS HTTP tmsHttpClientCancel(): requested");

    if (client == NULL) {
        LOG_ERROR("TMS HTTP tmsHttpClientCancel(): client is NULL");
        return;
    }

    if (!client->busy) {
        LOG_TRACE("TMS HTTP tmsHttpClientCancel(): ignored, no active request "
                  "state=%d",
                  (int)client->flow.state);
        return;
    }

    LOG_TRACE("TMS HTTP tmsHttpClientCancel(): cancelling state=%d starting=%d",
              (int)client->flow.state, (int)client->starting);

    /* Suppress callback delivery during cancellation. */
    client->busy     = false;
    client->onDone   = NULL;
    client->userData = NULL;

    httpFlowCancel(&client->flow);

    LOG_TRACE("TMS HTTP tmsHttpClientCancel(): cancel operation returned");
}

/* -----------------------------------------------------------
 * Release
 * ----------------------------------------------------------- */

/** @brief Release underlying HTTP flow resources. */
void tmsHttpClientRelease(TmsHttpClient* client) {
    LOG_TRACE("TMS HTTP tmsHttpClientRelease(): requested");

    if (client == NULL) {
        LOG_ERROR("TMS HTTP tmsHttpClientRelease(): client is NULL");
        return;
    }

    LOG_TRACE("TMS HTTP tmsHttpClientRelease(): releasing state=%d busy=%d",
              (int)client->flow.state, (int)client->busy);

    httpFlowRelease(&client->flow);

    client->busy     = false;
    client->onDone   = NULL;
    client->userData = NULL;

    LOG_TRACE("TMS HTTP tmsHttpClientRelease(): released state=%d",
              (int)client->flow.state);
}

/* -----------------------------------------------------------
 * Status
 * ----------------------------------------------------------- */

/**
 * @brief Return whether the TMS HTTP client has an active request.
 *
 * No trace here: this function may be called frequently in a polling loop.
 */
bool tmsHttpClientBusy(const TmsHttpClient* client) {
    return client != NULL && client->busy;
}
