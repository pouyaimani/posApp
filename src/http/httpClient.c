#include "httpClient.h"

#include <stdio.h>
#include <string.h>

#include "httpParserCfg.h"

static bool httpNthIsComplete(NthTransaction* tx, void* userData);

static int8_t httpNthReceive(NthTransaction* tx, void* userData);

static int8_t httpNthFailure(NthTransaction* tx, void* userData);

static void httpClientReset(HttpClient* client) {
    if (client == NULL)
        return;

    httpParserInit(&client->parser);

    client->state       = HTTP_CLIENT_IDLE;
    client->parseResult = HTTP_PARSE_INCOMPLETE;
    client->nthResult   = NTH_OK;
}

void httpClientInit(HttpClient* client, NthTransaction* tx, void* userData) {
    if (client == NULL)
        return;

    memset(client, 0, sizeof(*client));

    client->tx       = tx;
    client->userData = userData;

    httpParserInit(&client->parser);

    client->state = HTTP_CLIENT_IDLE;
}

static HttpClientResult appendString(char* buffer, size_t capacity,
                                     size_t* offset, const char* str) {
    size_t len;

    if (buffer == NULL || offset == NULL || str == NULL) {
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }

    len = strlen(str);

    if (*offset > capacity || len > capacity - *offset) {
        return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;
    }

    memcpy(buffer + *offset, str, len);

    *offset += len;

    return HTTP_CLIENT_OK;
}

static HttpClientResult appendHeader(char* buffer, size_t capacity,
                                     size_t* offset, const char* name,
                                     const char* value) {
    HttpClientResult result;

    result = appendString(buffer, capacity, offset, name);

    if (result != HTTP_CLIENT_OK)
        return result;

    result = appendString(buffer, capacity, offset, ": ");

    if (result != HTTP_CLIENT_OK)
        return result;

    result = appendString(buffer, capacity, offset, value);

    if (result != HTTP_CLIENT_OK)
        return result;

    return appendString(buffer, capacity, offset, "\r\n");
}

static HttpClientResult buildRequest(HttpClient* client, const char* method,
                                     const char* path, const char* host,
                                     const HttpRequestHeader* headers,
                                     size_t headerCount, const uint8_t* body,
                                     size_t bodyLen) {
    if (client == NULL || client->tx == NULL || method == NULL ||
        path == NULL || host == NULL) {
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }

    if (headerCount > 0 && headers == NULL)
        return HTTP_CLIENT_ERR_INVALID_ARG;

    if (bodyLen > 0 && body == NULL)
        return HTTP_CLIENT_ERR_INVALID_ARG;

    if (bodyLen > HTTP_MAX_BODY_SIZE)
        return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;

    ByteArray request;

    request.data     = client->tx->txBuffer.data;
    request.len      = 0;
    request.capacity = client->tx->txBuffer.capacity;

    size_t offset = 0;

    char line[HTTP_MAX_TARGET_LEN + 64U];

    int ret;

    /*
     * Request line.
     */
    ret = snprintf(line, sizeof(line), "%s %s HTTP/1.1\r\n", method, path);

    if (ret < 0 || (size_t)ret >= sizeof(line))
        return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;

    HttpClientResult result =
        appendString((char*)request.data, request.capacity, &offset, line);

    if (result != HTTP_CLIENT_OK)
        return result;

    /*
     * Host is mandatory for HTTP/1.1.
     */
    result = appendHeader((char*)request.data, request.capacity, &offset,
                          "Host", host);

    if (result != HTTP_CLIENT_OK)
        return result;

    /*
     * User headers.
     */
    for (size_t i = 0; i < headerCount; i++) {

        if (headers[i].name == NULL || headers[i].value == NULL) {
            return HTTP_CLIENT_ERR_INVALID_ARG;
        }

        result = appendHeader((char*)request.data, request.capacity, &offset,
                              headers[i].name, headers[i].value);

        if (result != HTTP_CLIENT_OK)
            return result;
    }

    /*
     * POST body.
     */
    if (body != NULL && bodyLen > 0) {

        char contentLength[32];

        ret = snprintf(contentLength, sizeof(contentLength), "%lu",
                       (unsigned long)bodyLen);

        if (ret < 0 || (size_t)ret >= sizeof(contentLength)) {
            return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;
        }

        result = appendHeader((char*)request.data, request.capacity, &offset,
                              "Content-Length", contentLength);

        if (result != HTTP_CLIENT_OK)
            return result;
    }

    /*
     * End headers.
     */
    result =
        appendString((char*)request.data, request.capacity, &offset, "\r\n");

    if (result != HTTP_CLIENT_OK)
        return result;

    /*
     * Body.
     */
    if (bodyLen > 0) {

        if (offset > request.capacity || bodyLen > request.capacity - offset) {
            return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;
        }

        memcpy(request.data + offset, body, bodyLen);

        offset += bodyLen;
    }

    request.len = offset;

    client->tx->txBuffer.len = request.len;

    client->state = HTTP_CLIENT_REQUEST_READY;

    return HTTP_CLIENT_OK;
}

HttpClientResult httpClientBuildGet(HttpClient* client, const char* path,
                                    const char*              host,
                                    const HttpRequestHeader* headers,
                                    size_t                   headerCount) {

    return buildRequest(client, "GET", path, host, headers, headerCount, NULL,
                        0);
}

HttpClientResult httpClientBuildPost(HttpClient* client, const char* path,
                                     const char*              host,
                                     const HttpRequestHeader* headers,
                                     size_t headerCount, const uint8_t* body,
                                     size_t bodyLen) {

    return buildRequest(client, "POST", path, host, headers, headerCount, body,
                        bodyLen);
}

HttpClientResult httpClientSend(HttpClient* client) {
    if (client == NULL || client->tx == NULL) {
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }

    if (client->state != HTTP_CLIENT_REQUEST_READY)
        return HTTP_CLIENT_ERR_INVALID_ARG;

    /*
     * Reset HTTP parser before starting a new response.
     */
    httpParserInit(&client->parser);

    client->parseResult = HTTP_PARSE_INCOMPLETE;
    client->nthResult   = NTH_OK;

    /*
     * Register HTTP callbacks with Nth.
     */
    client->tx->isComplete = httpNthIsComplete;
    client->tx->onReceive  = httpNthReceive;
    client->tx->onFailure  = httpNthFailure;

    client->tx->userData = client;

    NthResult result = nth()->sendProvidedTx(client->tx);

    if (result != NTH_OK) {

        client->nthResult = result;
        client->state     = HTTP_CLIENT_FAILED;

        return HTTP_CLIENT_ERR_NTH;
    }

    client->state = HTTP_CLIENT_REQUESTING;

    return HTTP_CLIENT_OK;
}

const HttpResponse* httpClientGetResponse(const HttpClient* client) {
    if (client == NULL)
        return NULL;

    if (!client->parser.headerComplete)
        return NULL;

    return &client->parser.response;
}

bool httpClientIsComplete(const HttpClient* client) {
    if (client == NULL)
        return false;

    return client->state == HTTP_CLIENT_COMPLETED;
}

bool httpClientIsFailed(const HttpClient* client) {
    if (client == NULL)
        return false;

    return client->state == HTTP_CLIENT_FAILED;
}

static bool httpNthIsComplete(NthTransaction* tx, void* userData) {
    HttpClient* client = (HttpClient*)userData;

    if (tx == NULL || client == NULL)
        return false;

    client->parseResult = httpParserParseResponse(
        &client->parser, tx->rxBuffer.data, tx->rxBuffer.len);

    switch (client->parseResult) {

    case HTTP_PARSE_OK:

        if (httpParserIsMessageComplete(&client->parser)) {
            client->state = HTTP_CLIENT_COMPLETED;
            return true;
        }

        /*
         * Headers are valid, but body is not complete yet.
         */
        client->state = HTTP_CLIENT_RESPONSE_RECEIVING;
        return false;

    case HTTP_PARSE_INCOMPLETE:

        client->state = HTTP_CLIENT_RESPONSE_RECEIVING;
        return false;

    case HTTP_PARSE_OVERFLOW:

        client->state     = HTTP_CLIENT_FAILED;
        client->nthResult = NTH_ERR_OVERFLOW;
        return true;

    case HTTP_PARSE_ERROR:
    default:

        client->state     = HTTP_CLIENT_FAILED;
        client->nthResult = NTH_ERR_INTERNAL;
        return true;
    }
}

static int8_t httpNthReceive(NthTransaction* tx, void* userData) {
    HttpClient* client = (HttpClient*)userData;

    if (tx == NULL || client == NULL)
        return -1;

    if (client->state != HTTP_CLIENT_COMPLETED)
        return 0;

    if (client->onComplete != NULL)
        client->onComplete(client, client->userData);

    return 0;
}

static int8_t httpNthFailure(NthTransaction* tx, void* userData) {
    HttpClient* client = (HttpClient*)userData;

    if (client == NULL)
        return -1;

    client->state = HTTP_CLIENT_FAILED;

    if (tx != NULL)
        client->nthResult = (NthResult)tx->lastError;

    if (client->onFailure != NULL)
        client->onFailure(client, client->userData);

    return 0;
}