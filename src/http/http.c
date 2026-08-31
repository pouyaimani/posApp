#include "http.h"

#include <stdio.h>
#include <string.h>

#include "nth/nth_config.h"

static const char* methodToString(HttpMethod method) {
    switch (method) {
    case HTTP_METHOD_GET:
        return "GET";

    case HTTP_METHOD_POST:
        return "POST";

    case HTTP_METHOD_PUT:
        return "PUT";

    case HTTP_METHOD_DELETE:
        return "DELETE";

    default:
        return NULL;
    }
}

static bool isBusyState(HttpState state) {
    return state == HTTP_STATE_CONNECTING || state == HTTP_STATE_SENDING ||
           state == HTTP_STATE_RECEIVING;
}

static bool appendString(char* buffer, size_t capacity, size_t* offset,
                         const char* str) {
    size_t len;

    if (!buffer || !offset || !str)
        return false;

    len = strlen(str);

    if (*offset > capacity || len > capacity - *offset) {
        return false;
    }

    memcpy(buffer + *offset, str, len);
    *offset += len;

    return true;
}

static bool appendBytes(char* buffer, size_t capacity, size_t* offset,
                        const void* data, size_t len) {
    if (!buffer || !offset || (!data && len != 0))
        return false;

    if (*offset > capacity || len > capacity - *offset) {
        return false;
    }

    if (len != 0)
        memcpy(buffer + *offset, data, len);

    *offset += len;

    return true;
}

static bool appendHeader(char* buffer, size_t capacity, size_t* offset,
                         const char* name, const char* value) {
    if (!appendString(buffer, capacity, offset, name))
        return false;

    if (!appendString(buffer, capacity, offset, ": "))
        return false;

    if (!appendString(buffer, capacity, offset, value))
        return false;

    if (!appendString(buffer, capacity, offset, "\r\n"))
        return false;

    return true;
}

static bool hasRequestHeader(const HttpRequest* request, const char* name) {
    if (!request || !name)
        return false;

    for (size_t i = 0; i < request->headerCount; ++i) {

        const char* a = request->headers[i].name;
        const char* b = name;

        while (*a && *b &&
               tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            ++a;
            ++b;
        }

        if (*a == '\0' && *b == '\0')
            return true;
    }

    return false;
}

static bool appendSizeDecimal(char* buffer, size_t capacity, size_t* offset,
                              size_t value) {
    char tmp[24];

    int ret = snprintf(tmp, sizeof(tmp), "%zu", value);

    if (ret < 0 || (size_t)ret >= sizeof(tmp)) {
        return false;
    }

    return appendString(buffer, capacity, offset, tmp);
}

static bool buildRequest(Http* http, const HttpRequest* request) {
    char*  buffer;
    size_t capacity;
    size_t offset = 0;

    const char* method;
    const char* version;

    if (!http || !request || !http->tx)
        return false;

    method = methodToString(request->method);

    if (!method)
        return false;

    if (!request->host || !request->path) {
        return false;
    }

    version = request->version == HTTP_VERSION_1_0 ? "HTTP/1.0" : "HTTP/1.1";

    buffer   = (char*)http->tx->txBuffer.data;
    capacity = http->tx->txBuffer.capacity;

    /*
     * Request line.
     */
    if (!appendString(buffer, capacity, &offset, method))
        return false;

    if (!appendString(buffer, capacity, &offset, " "))
        return false;

    if (!appendString(buffer, capacity, &offset, request->path))
        return false;

    if (!appendString(buffer, capacity, &offset, " "))
        return false;

    if (!appendString(buffer, capacity, &offset, version))
        return false;

    if (!appendString(buffer, capacity, &offset, "\r\n"))
        return false;

    /*
     * Host is mandatory for HTTP/1.1.
     */
    if (!hasRequestHeader(request, "Host")) {

        if (!appendHeader(buffer, capacity, &offset, "Host", request->host)) {
            return false;
        }
    }

    /*
     * User-provided headers.
     */
    for (size_t i = 0; i < request->headerCount; ++i) {

        const HttpRequestHeader* header = &request->headers[i];

        if (!header->name || !header->value) {
            return false;
        }

        if (!appendHeader(buffer, capacity, &offset, header->name,
                          header->value)) {
            return false;
        }
    }

    /*
     * Automatically generate Content-Length
     * when a body exists and the caller did not
     * explicitly provide one.
     */
    if (request->bodyLen > 0 && !hasRequestHeader(request, "Content-Length")) {

        if (!appendString(buffer, capacity, &offset, "Content-Length: "))
            return false;

        if (!appendSizeDecimal(buffer, capacity, &offset, request->bodyLen))
            return false;

        if (!appendString(buffer, capacity, &offset, "\r\n"))
            return false;
    }

    /*
     * HTTP client uses connection-close.
     *
     * This keeps the connection lifecycle simple and
     * prevents persistent-connection state from leaking
     * between independent NTH transactions.
     */
    if (!hasRequestHeader(request, "Connection")) {

        if (!appendString(buffer, capacity, &offset, "Connection: close\r\n"))
            return false;
    }

    /*
     * End of headers.
     */
    if (!appendString(buffer, capacity, &offset, "\r\n"))
        return false;

    /*
     * Request body.
     */
    if (request->bodyLen > 0) {

        if (!appendBytes(buffer, capacity, &offset, request->body,
                         request->bodyLen)) {
            return false;
        }
    }

    http->tx->txBuffer.len = offset;

    return true;
}

static void httpFail(Http* http, HttpResult error) {
    if (!http)
        return;

    http->error = error;
    http->state = HTTP_STATE_FAILED;

    if (http->onError)
        http->onError(http, error, http->userData);
}

static int8_t httpIsComplete(NthTransaction* tx, void* userData) {
    Http* http = (Http*)userData;

    if (!http || !tx)
        return 0;

    HttpParseResult result =
        httpParserFeed(&http->parser, tx->rxBuffer.data, tx->rxBuffer.len);

    if (result == HTTP_PARSE_COMPLETE_RESULT)
        return 1;

    if (result < 0) {
        http->error = HTTP_ERR_PARSE;
        return 1;
    }

    return 0;
}

static int8_t httpOnConnect(NthTransaction* tx, void* userData) {
    Http* http = (Http*)userData;

    (void)tx;

    if (!http)
        return 0;

    http->state = HTTP_STATE_SENDING;

    return 0;
}

static int8_t httpOnSent(NthTransaction* tx, void* userData) {
    Http* http = (Http*)userData;

    (void)tx;

    if (!http)
        return 0;

    http->state = HTTP_STATE_RECEIVING;

    return 0;
}

static int8_t httpOnReceive(NthTransaction* tx, void* userData) {
    Http* http = (Http*)userData;

    if (!http || !tx)
        return 0;

    if (http->error != HTTP_OK) {
        httpFail(http, http->error);
        return 0;
    }

    HttpParseResult result = httpParserParseComplete(
        &http->parser, tx->rxBuffer.data, tx->rxBuffer.len);

    if (result != HTTP_PARSE_COMPLETE_RESULT) {

        httpFail(http, HTTP_ERR_PARSE);

        return 0;
    }

    /*
     * Populate public response view.
     *
     * Body points directly into NTH RX storage.
     */
    http->response.statusCode = http->parser.statusCode;

    http->response.reasonPhrase = http->parser.reasonPhrase;

    http->response.headers = http->parser.headers;

    http->response.headerCount = http->parser.headerCount;

    http->response.body = tx->rxBuffer.data + http->parser.bodyOffset;

    http->response.bodyLen = http->parser.bodyReceived;

    /*
     * 2xx/3xx/4xx/5xx all complete normally from
     * the transport perspective. HTTP status is handled
     * separately from transport failure.
     */
    http->state = HTTP_STATE_COMPLETED;

    if (http->onComplete) {
        http->onComplete(http, &http->response, http->userData);
    }

    return 0;
}

static int8_t httpOnFailure(NthTransaction* tx, void* userData) {
    Http* http = (Http*)userData;

    if (!http || !tx)
        return 0;

    switch (tx->lastError) {

    case NTH_ERR_CONNECT:
        httpFail(http, HTTP_ERR_NTH_CONNECT);
        break;

    case NTH_ERR_SEND:
        httpFail(http, HTTP_ERR_NTH_SEND);
        break;

    case NTH_ERR_RECEIVE:
        httpFail(http, HTTP_ERR_NTH_RECEIVE);
        break;

    default:
        httpFail(http, HTTP_ERR_PARSE);
        break;
    }

    return 0;
}

static int8_t httpOnTimeout(NthTransaction* tx, void* userData) {
    Http* http = (Http*)userData;

    (void)tx;

    if (!http)
        return 0;

    httpFail(http, HTTP_ERR_TIMEOUT);

    return 0;
}

void httpInit(Http* http) {
    if (!http)
        return;

    memset(http, 0, sizeof(*http));

    http->state = HTTP_STATE_IDLE;
    http->error = HTTP_OK;
}

HttpResult httpRequest(Http* http, const HttpRequest* request) {
    if (!http || !request)
        return HTTP_ERR_INVALID_ARG;

    if (!request->host || !request->path) {
        return HTTP_ERR_INVALID_ARG;
    }

    if (request->headerCount > HTTP_MAX_REQUEST_HEADERS) {
        return HTTP_ERR_INVALID_ARG;
    }

    if (isBusyState(http->state))
        return HTTP_ERR_BUSY;

    /*
     * Allocate NTH transaction.
     */
    http->tx = nth()->alloc();

    if (!http->tx) {
        http->state = HTTP_STATE_FAILED;
        http->error = HTTP_ERR_NTH_ALLOC;
        return HTTP_ERR_NTH_ALLOC;
    }

    http->request = *request;

    /*
     * Validate body pointer.
     */
    if (request->bodyLen > 0 && !request->body) {

        nth()->release(http->tx);

        http->tx = NULL;

        http->state = HTTP_STATE_FAILED;
        http->error = HTTP_ERR_INVALID_ARG;

        return HTTP_ERR_INVALID_ARG;
    }

    /*
     * Build request directly into NTH TX buffer.
     */
    if (!buildRequest(http, request)) {

        nth()->release(http->tx);

        http->tx = NULL;

        http->state = HTTP_STATE_FAILED;
        http->error = HTTP_ERR_REQUEST_TOO_LARGE;

        return HTTP_ERR_REQUEST_TOO_LARGE;
    }

    /*
     * Initialize parser before connecting.
     */
    httpParserInit(&http->parser);

    http->response.statusCode   = 0;
    http->response.reasonPhrase = NULL;
    http->response.headers      = NULL;
    http->response.headerCount  = 0;
    http->response.body         = NULL;
    http->response.bodyLen      = 0;

    http->error = HTTP_OK;
    http->state = HTTP_STATE_CONNECTING;

    /*
     * Configure NTH callbacks.
     */
    http->tx->owner    = NULL;
    http->tx->userData = http;

    http->tx->onConnect  = httpOnConnect;
    http->tx->onSent     = httpOnSent;
    http->tx->onReceive  = httpOnReceive;
    http->tx->onFailure  = httpOnFailure;
    http->tx->onTimeout  = httpOnTimeout;
    http->tx->isComplete = httpIsComplete;

    /*
     * Connect.
     */
    NthResult result = nth()->connect(http->tx, request->host, request->port);

    if (result != NTH_OK) {

        nth()->release(http->tx);

        http->tx = NULL;

        http->state = HTTP_STATE_FAILED;
        http->error = HTTP_ERR_NTH_CONNECT;

        return HTTP_ERR_NTH_CONNECT;
    }

    return HTTP_OK;
}

HttpResult httpGet(Http* http, const char* host, uint16_t port,
                   const char* path) {
    HttpRequest request;

    memset(&request, 0, sizeof(request));

    request.method  = HTTP_METHOD_GET;
    request.host    = host;
    request.port    = port;
    request.path    = path;
    request.version = HTTP_VERSION_1_1;

    return httpRequest(http, &request);
}

HttpResult httpPost(Http* http, const char* host, uint16_t port,
                    const char* path, const char* contentType,
                    const uint8_t* body, size_t bodyLen) {
    HttpRequest request;

    memset(&request, 0, sizeof(request));

    request.method  = HTTP_METHOD_POST;
    request.host    = host;
    request.port    = port;
    request.path    = path;
    request.version = HTTP_VERSION_1_1;

    if (contentType) {
        request.headers[0].name = "Content-Type";

        request.headers[0].value = contentType;

        request.headerCount = 1;
    }

    request.body    = body;
    request.bodyLen = bodyLen;

    return httpRequest(http, &request);
}

void httpSetCallbacks(Http* http, HttpCompleteCallback onComplete,
                      HttpErrorCallback onError, void* userData) {
    if (!http)
        return;

    http->onComplete = onComplete;
    http->onError    = onError;
    http->userData   = userData;
}

void httpTick(Http* http) {
    if (!http)
        return;

    if (http->state == HTTP_STATE_COMPLETED ||
        http->state == HTTP_STATE_FAILED) {
        return;
    }

    /*
     * NTH is globally ticked by the application.
     *
     * Do NOT call nth()->tick() here if the system already
     * calls it elsewhere.
     */
}

void httpAbort(Http* http) {
    if (!http)
        return;

    if (http->tx) {
        nth()->release(http->tx);
        http->tx = NULL;
    }

    http->state = HTTP_STATE_IDLE;
    http->error = HTTP_OK;
}

void httpRelease(Http* http) {
    if (!http)
        return;

    if (http->tx) {
        nth()->release(http->tx);
        http->tx = NULL;
    }

    http->state = HTTP_STATE_IDLE;
    http->error = HTTP_OK;
}

bool httpIsBusy(const Http* http) {
    if (!http)
        return false;

    return isBusyState(http->state);
}

const HttpResponse* httpGetResponse(const Http* http) {
    if (!http)
        return NULL;

    if (http->state != HTTP_STATE_COMPLETED)
        return NULL;

    return &http->response;
}

const HttpParsedHeader* httpGetResponseHeader(const Http* http,
                                              const char* name) {
    if (!http || !name)
        return NULL;

    return httpParserGetHeader(&http->parser, name);
}

const char* httpGetErrorString(HttpResult error) {
    switch (error) {

    case HTTP_OK:
        return "OK";

    case HTTP_ERR_INVALID_ARG:
        return "invalid argument";

    case HTTP_ERR_BUSY:
        return "HTTP client busy";

    case HTTP_ERR_NTH_ALLOC:
        return "NTH transaction allocation failed";

    case HTTP_ERR_REQUEST_TOO_LARGE:
        return "HTTP request too large";

    case HTTP_ERR_INVALID_HOST:
        return "invalid host";

    case HTTP_ERR_REQUEST_BUILD:
        return "HTTP request build failed";

    case HTTP_ERR_NTH_CONNECT:
        return "NTH connection failed";

    case HTTP_ERR_NTH_SEND:
        return "NTH send failed";

    case HTTP_ERR_NTH_RECEIVE:
        return "NTH receive failed";

    case HTTP_ERR_TIMEOUT:
        return "HTTP timeout";

    case HTTP_ERR_PARSE:
        return "HTTP response parse failed";

    case HTTP_ERR_RESPONSE_TOO_LARGE:
        return "HTTP response too large";

    case HTTP_ERR_UNSUPPORTED_RESPONSE:
        return "unsupported HTTP response";

    case HTTP_ERR_STATUS:
        return "HTTP status indicates failure";

    default:
        return "unknown HTTP error";
    }
}