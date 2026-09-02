#include "tmsHttpClient.h"

#include <string.h>

static void httpFinished(HttpFlow *flow, const HttpFlowStatus *status)
{
    TmsHttpClient *client;
    TmsHttpDoneCallback callback;
    void *userData;
    const HttpResponse *response;
    const uint8_t *body;
    size_t bodyLength;

    if (flow == NULL || status == NULL)
        return;

    client = (TmsHttpClient *)flow->userData;
    if (client == NULL)
        return;

    response = httpFlowGetResponse(flow);
    body = httpFlowGetBody(flow);
    bodyLength = httpFlowGetBodyLength(flow);
    callback = client->onDone;
    userData = client->userData;
    client->busy = false;

    if (callback != NULL)
        callback(client, status->result, response, body, bodyLength, userData);
}

void tmsHttpClientInit(TmsHttpClient *client, const char *host, uint16_t port)
{
    if (client == NULL)
        return;
    memset(client, 0, sizeof(*client));
    if (host != NULL) {
        strncpy(client->host, host, sizeof(client->host) - 1u);
        client->host[sizeof(client->host) - 1u] = '\0';
    }
    client->port = port;
    httpFlowInit(&client->flow);
}

HttpFlowResult tmsHttpClientRequest(TmsHttpClient *client, HttpMethod method,
                                    const char *path,
                                    const HttpFlowHeader *headers,
                                    size_t headerCount, const uint8_t *body,
                                    size_t bodyLength,
                                    TmsHttpDoneCallback callback,
                                    void *userData)
{
    HttpFlowRequest request;
    HttpFlowCallbacks callbacks;
    HttpFlowResult result;

    if (client == NULL || path == NULL || callback == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;
    if (client->busy)
        return HTTP_FLOW_ERR_BUSY;

    if (client->flow.state != HTTP_FLOW_IDLE)
        httpFlowRelease(&client->flow);

    memset(&request, 0, sizeof(request));
    request.method = method;
    request.path = path;
    request.headers = headers;
    request.headerCount = headerCount;
    request.body = body;
    request.bodyLength = bodyLength;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.onDone = httpFinished;

    client->onDone = callback;
    client->userData = userData;
    client->busy = true;
    result = httpFlowStart(&client->flow, NULL, client->host, client->port,
                           &request, &callbacks, client);
    if (result != HTTP_FLOW_OK)
        client->busy = false;
    return result;
}

HttpFlowResult tmsHttpClientPostJson(TmsHttpClient *client, const char *path,
                                     const char *json,
                                     TmsHttpDoneCallback callback,
                                     void *userData)
{
    static const HttpFlowHeader headers[] = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"Connection", "close"}
    };
    if (json == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;
    return tmsHttpClientRequest(client, HTTP_METHOD_POST, path, headers,
                                sizeof(headers) / sizeof(headers[0]),
                                (const uint8_t *)json, strlen(json), callback,
                                userData);
}

HttpFlowResult tmsHttpClientGet(TmsHttpClient *client, const char *path,
                                TmsHttpDoneCallback callback, void *userData)
{
    static const HttpFlowHeader headers[] = {
        {"Accept", "application/json"}, {"Connection", "close"}
    };
    return tmsHttpClientRequest(client, HTTP_METHOD_GET, path, headers,
                                sizeof(headers) / sizeof(headers[0]), NULL, 0,
                                callback, userData);
}

void tmsHttpClientCancel(TmsHttpClient *client)
{
    if (client == NULL || !client->busy)
        return;
    client->busy = false;
    httpFlowCancel(&client->flow);
}

void tmsHttpClientRelease(TmsHttpClient *client)
{
    if (client == NULL)
        return;
    httpFlowRelease(&client->flow);
    client->busy = false;
    client->onDone = NULL;
    client->userData = NULL;
}

bool tmsHttpClientBusy(const TmsHttpClient *client)
{
    return client != NULL && client->busy;
}
