#include "httpFlow.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "error.h"
#include "logger.h"

#if !defined(NT_RX_BUFFER_SIZE)
#error "NT_RX_BUFFER_SIZE must be defined"
#endif

#if (HTTP_MAX_HEADER_BYTES + HTTP_MAX_BODY_SIZE) > NT_RX_BUFFER_SIZE
#error "NTH RX buffer cannot contain maximum HTTP headers and body"
#endif

/*
 * NTH callbacks
 */
static int8_t httpFlowOnConnect(NthTransaction* tx, void* userData);

static int8_t httpFlowOnSent(NthTransaction* tx, void* userData);

static NthRxDecision httpFlowNthIsComplete(NthTransaction* tx, void* userData);

static int8_t httpFlowNthReceive(NthTransaction* tx, void* userData);

static int8_t httpFlowOnFailure(NthTransaction* tx, void* userData);

static int8_t httpFlowOnTimeout(NthTransaction* tx, void* userData);

/*
 * Private helpers
 */

static void httpFlowSetState(HttpFlow* flow, HttpFlowState state) {
    if (flow == NULL)
        return;

    if (flow->state == state)
        return;

    LOG_TRACE("HTTP flow state %d -> %d", flow->state, state);

    flow->state = state;
}

static bool httpFlowIsTerminalState(const HttpFlow* flow) {
    if (flow == NULL)
        return true;

    return flow->state == HTTP_FLOW_COMPLETED ||
           flow->state == HTTP_FLOW_FAILED;
}

static void httpFlowFail(HttpFlow* flow, HttpFlowResult result) {
    if (flow == NULL || httpFlowIsTerminalState(flow))
        return;

    flow->status.result = result;

    httpFlowSetState(flow, HTTP_FLOW_FAILED);

    if (flow->callbacks.onDone != NULL)
        flow->callbacks.onDone(flow, &flow->status);
}

static void httpFlowComplete(HttpFlow* flow) {
    if (flow == NULL || httpFlowIsTerminalState(flow))
        return;

    flow->status.result = HTTP_FLOW_OK;

    httpFlowSetState(flow, HTTP_FLOW_COMPLETED);

    if (flow->callbacks.onDone != NULL)
        flow->callbacks.onDone(flow, &flow->status);
}

static bool httpIsValidHeaderName(const char* name) {
    if (name == NULL)
        return false;

    while (*name) {

        char c = *name;

        /*
         * HTTP token characters only.
         */
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') {
            return false;
        }

        name++;
    }

    return true;
}

static bool httpIsValidHeaderValue(const char* value) {
    if (value == NULL)
        return false;

    while (*value) {

        if (*value == '\r' || *value == '\n') {
            return false;
        }

        value++;
    }

    return true;
}

static bool httpIsValidMethod(HttpMethod method) {
    switch (method) {

    case HTTP_METHOD_GET:
    case HTTP_METHOD_POST:
    case HTTP_METHOD_PUT:
    case HTTP_METHOD_DELETE:
        return true;

    default:
        return false;
    }
}

static const char* httpMethodString(HttpMethod method) {
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

/*
 * Append string into ByteArray.
 */
static bool httpAppend(ByteArray* buffer, const char* str) {
    if (buffer == NULL || str == NULL) {
        return false;
    }

    size_t len = strlen(str);

    if (buffer->len > buffer->capacity)
        return false;

    if (len > buffer->capacity - buffer->len)
        return false;

    memcpy(buffer->data + buffer->len, str, len);

    buffer->len += len;

    return true;
}

static bool httpAppendHeader(ByteArray* buffer, const char* name,
                             const char* value) {
    if (!httpIsValidHeaderName(name))
        return false;

    if (!httpIsValidHeaderValue(value))
        return false;

    if (!httpAppend(buffer, name))
        return false;

    if (!httpAppend(buffer, ": "))
        return false;

    if (!httpAppend(buffer, value))
        return false;

    if (!httpAppend(buffer, "\r\n"))
        return false;

    return true;
}

static HttpFlowResult httpBuildRequest(HttpFlow* flow) {
    if (flow == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;

    const HttpFlowRequest* req = &flow->request;

    ByteArray* buffer = &flow->requestBuffer;

    const char* method = httpMethodString(req->method);

    if (method == NULL || req->path == NULL || req->host == NULL) {
        return HTTP_FLOW_ERR_REQUEST;
    }

    char line[320];

    int ret =
        snprintf(line, sizeof(line), "%s %s HTTP/1.1\r\n", method, req->path);

    if (ret < 0 || (size_t)ret >= sizeof(line)) {
        return HTTP_FLOW_ERR_REQUEST;
    }

    if (!httpAppend(buffer, line))
        return HTTP_FLOW_ERR_REQUEST;

    /*
     * Mandatory HTTP/1.1 Host
     */
    {
        char hostValue[HTTP_MAX_HEADER_VALUE_LEN + 1U];
        int  written;

        if (req->port == 80U) {
            written = snprintf(hostValue, sizeof(hostValue), "%s", req->host);
        } else {
            written = snprintf(hostValue, sizeof(hostValue), "%s:%u", req->host,
                               (unsigned int)req->port);
        }

        if (written < 0 || (size_t)written >= sizeof(hostValue)) {
            return HTTP_FLOW_ERR_REQUEST;
        }

        if (!httpAppendHeader(buffer, "Host", hostValue)) {
            return HTTP_FLOW_ERR_REQUEST;
        }
    }

    /*
     * User headers
     */
    for (size_t i = 0; i < req->headerCount; i++) {

        if (!httpAppendHeader(buffer, req->headers[i].name,
                              req->headers[i].value)) {
            return HTTP_FLOW_ERR_REQUEST;
        }
    }

    /*
     * Body
     */
    if (req->bodyLength > 0) {

        char lenBuffer[32];

        snprintf(lenBuffer, sizeof(lenBuffer), "%lu",
                 (unsigned long)req->bodyLength);

        if (!httpAppendHeader(buffer, "Content-Length", lenBuffer)) {
            return HTTP_FLOW_ERR_REQUEST;
        }
    }

    /*
     * End headers
     */
    if (!httpAppend(buffer, "\r\n")) {
        return HTTP_FLOW_ERR_REQUEST;
    }

    /*
     * Append body
     */
    if (req->bodyLength > 0) {

        if (buffer->len > buffer->capacity)
            return HTTP_FLOW_ERR_REQUEST;

        if (req->bodyLength > buffer->capacity - buffer->len) {
            return HTTP_FLOW_ERR_REQUEST;
        }

        memcpy(buffer->data + buffer->len, req->body, req->bodyLength);

        buffer->len += req->bodyLength;
    }

    return HTTP_FLOW_OK;
}

void httpFlowInit(HttpFlow* flow) {
    if (flow == NULL)
        return;

    memset(flow, 0, sizeof(*flow));

    flow->state = HTTP_FLOW_IDLE;

    httpParserInit(&flow->parser);

    flow->status.result = HTTP_FLOW_OK;
}

HttpFlowResult httpFlowStart(HttpFlow* flow, State* owner, const char* host,
                             uint16_t port, const HttpFlowRequest* request,
                             const HttpFlowCallbacks* callbacks,
                             void*                    userData) {

    if (flow == NULL || request == NULL || host == NULL) {
        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    if (flow->state != HTTP_FLOW_IDLE) {
        return HTTP_FLOW_ERR_BUSY;
    }

    if (!httpIsValidMethod(request->method)) {
        return HTTP_FLOW_ERR_REQUEST;
    }

    /*
     * Store request information.
     */
    memcpy(&flow->request, request, sizeof(HttpFlowRequest));

    flow->request.host = host;

    flow->request.port = port;

    memset(&flow->status, 0, sizeof(flow->status));
    memset(&flow->callbacks, 0, sizeof(flow->callbacks));

    flow->status.result = HTTP_FLOW_OK;

    httpParserInit(&flow->parser);

    if (callbacks != NULL) {
        memcpy(&flow->callbacks, callbacks, sizeof(flow->callbacks));
    }

    flow->owner = owner;

    flow->userData = userData;

    flow->tx = nth()->alloc();

    if (flow->tx == NULL) {
        return HTTP_FLOW_ERR_NO_RESOURCE;
    }

    flow->ownsTransaction = true;

    flow->tx->owner = owner;

    flow->tx->userData = flow;

    flow->tx->onConnect = httpFlowOnConnect;

    flow->tx->onSent = httpFlowOnSent;

    flow->tx->isComplete = httpFlowNthIsComplete;

    flow->tx->onReceive = httpFlowNthReceive;

    flow->tx->onChunk = NULL;

    flow->tx->onFailure = httpFlowOnFailure;

    flow->tx->onTimeout = httpFlowOnTimeout;

    /*
     * Use NTH TX buffer
     * for HTTP request.
     */
    flow->requestBuffer.data = flow->tx->txBuffer.data;

    flow->requestBuffer.capacity = flow->tx->txBuffer.capacity;

    flow->requestBuffer.len = 0;

    httpFlowSetState(flow, HTTP_FLOW_BUILDING);

    HttpFlowResult buildResult = httpBuildRequest(flow);

    if (buildResult != HTTP_FLOW_OK) {
        httpFlowFail(flow, buildResult);

        return buildResult;
    }

    flow->tx->txBuffer.len = flow->requestBuffer.len;

    httpFlowSetState(flow, HTTP_FLOW_CONNECTING);

    NthResult nthResult = nth()->connect(flow->tx, host, port);

    if (nthResult != NTH_OK) {

        flow->status.nthResult = nthResult;

        httpFlowFail(flow, HTTP_FLOW_ERR_CONNECT);

        return HTTP_FLOW_ERR_CONNECT;
    }

    return HTTP_FLOW_OK;
}

/*
 * ============================================================
 * NTH callbacks
 * ============================================================
 */

static int8_t httpFlowOnConnect(NthTransaction* tx, void* userData) {
    if (tx == NULL || userData == NULL)
        return -1;

    HttpFlow* flow = (HttpFlow*)userData;

    httpFlowSetState(flow, HTTP_FLOW_SENDING);

    /*
     * Request has already been built.
     *
     * Start sending.
     */
    NthResult result = nth()->sendProvidedTx(tx);

    if (result != NTH_OK) {
        flow->status.nthResult = result;

        httpFlowFail(flow, HTTP_FLOW_ERR_SEND);

        return -1;
    }

    return 0;
}

static int8_t httpFlowOnSent(NthTransaction* tx, void* userData) {
    if (tx == NULL || userData == NULL)
        return -1;

    HttpFlow* flow = (HttpFlow*)userData;

    httpFlowSetState(flow, HTTP_FLOW_RECEIVING_HEADERS);

    /*
     * HTTP response parser starts empty.
     */
    httpParserInit(&flow->parser);

    return 0;
}

static NthRxDecision httpFlowNthIsComplete(NthTransaction* tx, void* userData) {
    HttpFlow*       flow;
    HttpParseResult parseResult;

    if (tx == NULL || userData == NULL)
        return NTH_RX_ERROR;

    flow = (HttpFlow*)userData;

    if (httpFlowIsTerminalState(flow))
        return flow->state == HTTP_FLOW_COMPLETED ? NTH_RX_COMPLETE
                                                  : NTH_RX_ERROR;

    /*
     * Only a clean peer closure may finish a close-delimited body.
     * Socket errors and timeouts arrive through separate callbacks.
     */
    if (tx->peerClosed) {
        parseResult = httpParserFinish(&flow->parser);
    } else {
        parseResult = httpParserParseResponse(&flow->parser, tx->rxBuffer.data,
                                              tx->rxBuffer.len);
    }

    flow->status.parseResult = parseResult;

    if (httpParserIsHeaderComplete(&flow->parser)) {

        const HttpResponse* response = httpParserGetResponse(&flow->parser);

        if (response != NULL)
            flow->status.httpStatus = response->statusCode;

        if (!httpParserIsMessageComplete(&flow->parser))
            httpFlowSetState(flow, HTTP_FLOW_RECEIVING_BODY);
    }

    switch (parseResult) {

    case HTTP_PARSE_OK:
        return httpParserIsMessageComplete(&flow->parser) ? NTH_RX_COMPLETE
                                                          : NTH_RX_MORE;

    case HTTP_PARSE_INCOMPLETE:

        /*
         * EOF means no more bytes can arrive.
         */
        return tx->peerClosed ? NTH_RX_ERROR : NTH_RX_MORE;

    case HTTP_PARSE_ERROR:
    case HTTP_PARSE_OVERFLOW:
    case HTTP_PARSE_UNSUPPORTED:
    default:
        return NTH_RX_ERROR;
    }
}

static int8_t httpFlowNthReceive(NthTransaction* tx, void* userData) {
    HttpFlow*           flow;
    const HttpResponse* response;
    const uint8_t*      body;
    size_t              bodyLength;

    if (tx == NULL || userData == NULL)
        return -1;

    flow = (HttpFlow*)userData;

    if (httpFlowIsTerminalState(flow))
        return 0;

    if (!httpParserIsMessageComplete(&flow->parser)) {
        httpFlowFail(flow, HTTP_FLOW_ERR_PARSE);
        return -1;
    }

    response = httpParserGetResponse(&flow->parser);

    if (response == NULL) {
        httpFlowFail(flow, HTTP_FLOW_ERR_PARSE);
        return -1;
    }

    flow->status.httpStatus = response->statusCode;

    if (flow->callbacks.onHeaders != NULL)
        flow->callbacks.onHeaders(flow, response);

    if (httpFlowIsTerminalState(flow))
        return 0;

    body =
        httpParserGetBody(&flow->parser, tx->rxBuffer.data, tx->rxBuffer.len);

    bodyLength = httpParserGetBodyLength(&flow->parser, tx->rxBuffer.len);

    if (body != NULL && bodyLength > 0U && flow->callbacks.onBody != NULL) {

        httpFlowSetState(flow, HTTP_FLOW_RECEIVING_BODY);

        if (flow->callbacks.onBody(flow, body, bodyLength) != 0) {

            httpFlowFail(flow, HTTP_FLOW_ERR_BODY);
            return -1;
        }
    }

    if (httpFlowIsTerminalState(flow))
        return 0;

    /*
     * Preserve your current policy:
     * non-2xx responses complete as HTTP errors.
     * Status 206 is included in the successful range.
     */
    if (response->statusCode < 200 || response->statusCode >= 300) {

        httpFlowFail(flow, HTTP_FLOW_ERR_HTTP_STATUS);

        return 0;
    }

    httpFlowComplete(flow);

    return 0;
}

static int8_t httpFlowOnFailure(NthTransaction* tx, void* userData) {
    HttpFlow*      flow;
    HttpFlowResult result = HTTP_FLOW_ERR_RECEIVE;

    if (userData == NULL)
        return -1;

    flow = (HttpFlow*)userData;

    if (httpFlowIsTerminalState(flow))
        return 0;

    if (tx != NULL)
        flow->status.nthResult = tx->lastError;

    switch (flow->status.parseResult) {

    case HTTP_PARSE_OVERFLOW:
        result = HTTP_FLOW_ERR_BODY;
        break;

    case HTTP_PARSE_ERROR:
    case HTTP_PARSE_UNSUPPORTED:
        result = HTTP_FLOW_ERR_PARSE;
        break;

    case HTTP_PARSE_OK:
    case HTTP_PARSE_INCOMPLETE:
    default:
        result = HTTP_FLOW_ERR_RECEIVE;
        break;
    }

    httpFlowFail(flow, result);

    return 0;
}

static int8_t httpFlowOnTimeout(NthTransaction* tx, void* userData) {
    if (userData == NULL)
        return -1;

    HttpFlow* flow = (HttpFlow*)userData;

    if (tx) {
        flow->status.nthResult = tx->lastError;
    }

    httpFlowFail(flow, HTTP_FLOW_ERR_TIMEOUT);

    return 0;
}

/*
 * ============================================================
 * Public API
 * ============================================================
 */

void httpFlowCancel(HttpFlow* flow) {
    if (flow == NULL)
        return;

    if (flow->tx) {
        nth()->disconnect(flow->tx);
    }

    httpFlowFail(flow, HTTP_FLOW_ERR_INTERNAL);
}

void httpFlowRelease(HttpFlow* flow) {
    if (flow == NULL)
        return;

    if (flow->tx && flow->ownsTransaction) {

        nth()->release(flow->tx);
    }

    flow->tx = NULL;

    flow->state = HTTP_FLOW_IDLE;
}

bool httpFlowIsComplete(const HttpFlow* flow) {
    if (flow == NULL)
        return false;

    return flow->state == HTTP_FLOW_COMPLETED;
}

bool httpFlowIsFailed(const HttpFlow* flow) {
    if (flow == NULL)
        return false;

    return flow->state == HTTP_FLOW_FAILED;
}

const HttpResponse* httpFlowGetResponse(const HttpFlow* flow) {
    if (flow == NULL)
        return NULL;

    if (!flow->parser.headerComplete)
        return NULL;

    return &flow->parser.response;
}

const uint8_t* httpFlowGetBody(const HttpFlow* flow) {
    if (flow == NULL || flow->tx == NULL) {
        return NULL;
    }

    return httpParserGetBody(&flow->parser, flow->tx->rxBuffer.data,
                             flow->tx->rxBuffer.len);
}

size_t httpFlowGetBodyLength(const HttpFlow* flow) {
    if (flow == NULL || flow->tx == NULL) {
        return 0;
    }

    return httpParserGetBodyLength(&flow->parser, flow->tx->rxBuffer.len);
}