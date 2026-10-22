#include "httpFlow.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "error.h"
#include "logger.h"
#include "utility/utility.h"

#if !defined(NT_RX_BUFFER_SIZE)
#error "NT_RX_BUFFER_SIZE must be defined"
#endif

#if (HTTP_MAX_HEADER_BYTES + HTTP_MAX_BODY_SIZE) > NT_RX_BUFFER_SIZE
#error "NTH RX buffer cannot contain maximum HTTP headers and body"
#endif

/*
 * Debug-only HTTP wire logging.
 *
 * WARNING: Bodies can contain customer data, session tokens, PIN-related
 * material or device identifiers. Disable all wire dumps in production:
 *   -DHTTP_FLOW_TRACE_WIRE=0
 *
 * HTTP_FLOW_TRACE_BODY=1 prints a bounded BODY (including JSON) without
 * redacting its fields. Use ONLY on a controlled development terminal.
 * Authorization/Cookie/API-key headers are masked by the header formatter.
 * A binary body is dumped as HEX rather than treated as a C string.
 */
#ifndef HTTP_FLOW_TRACE_WIRE
#define HTTP_FLOW_TRACE_WIRE 1
#endif
#ifndef HTTP_FLOW_TRACE_BODY
#define HTTP_FLOW_TRACE_BODY 1
#endif
#ifndef HTTP_FLOW_TRACE_MAX_BYTES
#define HTTP_FLOW_TRACE_MAX_BYTES 2048u
#endif

/* NTH callbacks. */
static int8_t        httpFlowOnConnect(NthTransaction* tx, void* userData);
static int8_t        httpFlowOnSent(NthTransaction* tx, void* userData);
static NthRxDecision httpFlowNthIsComplete(NthTransaction* tx, void* userData);
static int8_t        httpFlowNthReceive(NthTransaction* tx, void* userData);
static int8_t        httpFlowOnFailure(NthTransaction* tx, void* userData);
static int8_t        httpFlowOnTimeout(NthTransaction* tx, void* userData);

/* -----------------------------------------------------------
 * Diagnostic helpers
 * ----------------------------------------------------------- */
static const char* httpStateName(HttpFlowState state) {
    switch (state) {
    case HTTP_FLOW_IDLE:
        return "IDLE";
    case HTTP_FLOW_BUILDING:
        return "BUILDING";
    case HTTP_FLOW_CONNECTING:
        return "CONNECTING";
    case HTTP_FLOW_SENDING:
        return "SENDING";
    case HTTP_FLOW_RECEIVING_HEADERS:
        return "RECEIVING_HEADERS";
    case HTTP_FLOW_RECEIVING_BODY:
        return "RECEIVING_BODY";
    case HTTP_FLOW_COMPLETED:
        return "COMPLETED";
    case HTTP_FLOW_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

static const char* httpParseResultName(HttpParseResult result) {
    switch (result) {
    case HTTP_PARSE_OK:
        return "OK";
    case HTTP_PARSE_INCOMPLETE:
        return "INCOMPLETE";
    case HTTP_PARSE_ERROR:
        return "ERROR";
    case HTTP_PARSE_OVERFLOW:
        return "OVERFLOW";
    case HTTP_PARSE_UNSUPPORTED:
        return "UNSUPPORTED";
    default:
        return "UNKNOWN";
    }
}

static void httpNotifyState(HttpFlow* flow, HttpFlowState state) {
    if (flow != NULL && flow->callbacks.onState != NULL)
        flow->callbacks.onState(flow, state);
}

#if HTTP_FLOW_TRACE_WIRE
/* Case-insensitive comparison of an exact header name (not its value). */
static bool httpTraceHeaderIs(const uint8_t* data, size_t nameLength,
                              const char* expected) {
    size_t i;
    if (data == NULL || expected == NULL || strlen(expected) != nameLength)
        return false;
    for (i = 0; i < nameLength; ++i) {
        if (tolower((unsigned char)data[i]) !=
            tolower((unsigned char)expected[i]))
            return false;
    }
    return true;
}

static bool httpTraceSensitiveHeader(const uint8_t* line, size_t length) {
    size_t nameLength = 0;
    while (nameLength < length && line[nameLength] != ':')
        ++nameLength;
    if (nameLength == length)
        return false;
    return httpTraceHeaderIs(line, nameLength, "authorization") ||
           httpTraceHeaderIs(line, nameLength, "proxy-authorization") ||
           httpTraceHeaderIs(line, nameLength, "cookie") ||
           httpTraceHeaderIs(line, nameLength, "set-cookie") ||
           httpTraceHeaderIs(line, nameLength, "x-api-key") ||
           httpTraceHeaderIs(line, nameLength, "x-auth-token");
}

/*
 * Print one HTTP line, in bounded chunks, with nonprintable bytes escaped.
 * No assumption that input has a terminating NUL byte.
 */
static void httpTraceText(const char* direction, const char* section,
                          const uint8_t* data, size_t length) {
    char   line[97];
    size_t used = 0u;
    size_t i;
    for (i = 0u; i < length; ++i) {
        unsigned char c = data[i];
        char          part[4];
        size_t        n;

        if (c == '\r') {
            part[0] = '\\';
            part[1] = 'r';
            n       = 2u;
        } else if (c == '\n') {
            part[0] = '\\';
            part[1] = 'n';
            n       = 2u;
        } else if (c == '\t') {
            part[0] = '\\';
            part[1] = 't';
            n       = 2u;
        } else if (c >= 32u && c <= 126u) {
            part[0] = (char)c;
            n       = 1u;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            part[0]                 = '\\';
            part[1]                 = 'x';
            part[2]                 = hex[c >> 4];
            part[3]                 = hex[c & 15u];
            n                       = 4u;
        }
        if (used + n >= sizeof(line)) {
            line[used] = '\0';
            LOG_TRACE("HTTP FLOW %s %s: %s", direction, section, line);
            used = 0u;
        }
        memcpy(line + used, part, n);
        used += n;
    }
    if (used != 0u) {
        line[used] = '\0';
        LOG_TRACE("HTTP FLOW %s %s: %s", direction, section, line);
    }
}

#if HTTP_FLOW_TRACE_BODY
static bool httpTraceIsText(const uint8_t* data, size_t length) {
    size_t i;
    size_t sample = length < 128u ? length : 128u;
    for (i = 0u; i < sample; ++i) {
        unsigned char c = data[i];
        if (c != '\n' && c != '\r' && c != '\t' && (c < 32u || c == 127u))
            return false;
    }
    return true;
}

static void httpTraceHex(const char* direction, const uint8_t* data,
                         size_t length) {
    static const char hex[] = "0123456789ABCDEF";
    size_t            offset;
    for (offset = 0u; offset < length; offset += 16u) {
        char   line[16u * 3u + 1u];
        size_t j, used = 0u;
        size_t count = length - offset < 16u ? length - offset : 16u;
        for (j = 0u; j < count; ++j) {
            uint8_t b    = data[offset + j];
            line[used++] = hex[b >> 4u];
            line[used++] = hex[b & 15u];
            line[used++] = ' ';
        }
        line[used] = '\0';
        LOG_TRACE("HTTP FLOW %s BODY: offset=%lu HEX=%s", direction,
                  (unsigned long)offset, line);
    }
}

#endif /* HTTP_FLOW_TRACE_BODY */

/* Print a bounded wire message: start line, headers, then body. */
static void httpTraceWire(const char* direction, const uint8_t* data,
                          size_t length) {
    size_t limit, headerEnd, cursor;
    bool   foundBoundary = false;

    if (data == NULL) {
        LOG_TRACE("HTTP FLOW %s WIRE: no data, len=%lu", direction,
                  (unsigned long)length);
        return;
    }
    limit = length < (size_t)HTTP_FLOW_TRACE_MAX_BYTES
                ? length
                : (size_t)HTTP_FLOW_TRACE_MAX_BYTES;
    LOG_TRACE("HTTP FLOW %s WIRE: length=%lu showing=%lu", direction,
              (unsigned long)length, (unsigned long)limit);

    /* Find CRLFCRLF in the ORIGINAL buffer within the logging limit. */
    headerEnd = limit;
    for (cursor = 0u; cursor + 3u < limit; ++cursor) {
        if (data[cursor] == '\r' && data[cursor + 1u] == '\n' &&
            data[cursor + 2u] == '\r' && data[cursor + 3u] == '\n') {
            headerEnd     = cursor + 4u;
            foundBoundary = true;
            break;
        }
    }

    /* Dump headers line-by-line so sensitive header values can be hidden. */
    cursor = 0u;
    while (cursor < headerEnd) {
        size_t lineEnd = cursor;
        while (lineEnd + 1u < headerEnd &&
               !(data[lineEnd] == '\r' && data[lineEnd + 1u] == '\n'))
            ++lineEnd;
        if (lineEnd + 1u >= headerEnd)
            lineEnd = headerEnd;
        if (lineEnd > cursor) {
            if (httpTraceSensitiveHeader(data + cursor, lineEnd - cursor)) {
                LOG_TRACE("HTTP FLOW %s HEADER: [sensitive header redacted]",
                          direction);
            } else {
                httpTraceText(direction, "HEADER", data + cursor,
                              lineEnd - cursor);
            }
        }
        cursor = (lineEnd + 1u < headerEnd) ? lineEnd + 2u : headerEnd;
    }

    if (foundBoundary) {
        size_t bodyLen = length - headerEnd;
        size_t shown   = limit - headerEnd;
        LOG_TRACE("HTTP FLOW %s BODY: length=%lu showing=%lu", direction,
                  (unsigned long)bodyLen, (unsigned long)shown);
#if HTTP_FLOW_TRACE_BODY
        if (shown > 0u) {
            if (httpTraceIsText(data + headerEnd, shown))
                httpTraceText(direction, "BODY", data + headerEnd, shown);
            else
                httpTraceHex(direction, data + headerEnd, shown);
        }
#else
        LOG_TRACE("HTTP FLOW %s BODY: logging disabled", direction);
#endif
    } else {
        LOG_TRACE("HTTP FLOW %s WIRE: incomplete/no header terminator within "
                  "trace limit",
                  direction);
    }
    if (limit < length)
        LOG_TRACE("HTTP FLOW %s WIRE: truncated %lu bytes (increase "
                  "HTTP_FLOW_TRACE_MAX_BYTES for debug)",
                  direction, (unsigned long)(length - limit));
}
#else
#define httpTraceWire(direction, data, length) ((void)0)
#endif /* HTTP_FLOW_TRACE_WIRE */

/* -----------------------------------------------------------
 * Flow state and completion
 * ----------------------------------------------------------- */
static void httpFlowSetState(HttpFlow* flow, HttpFlowState state) {
    if (flow == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowSetState(): flow=NULL");
        return;
    }
    if (flow->state == state)
        return;
    LOG_TRACE("HTTP FLOW state: %s(%d) -> %s(%d)", httpStateName(flow->state),
              (int)flow->state, httpStateName(state), (int)state);
    flow->state = state;
}

static bool httpFlowIsTerminalState(const HttpFlow* flow) {
    return flow == NULL || flow->state == HTTP_FLOW_COMPLETED ||
           flow->state == HTTP_FLOW_FAILED;
}

static void httpFlowFail(HttpFlow* flow, HttpFlowResult result) {
    if (flow == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowFail(): flow=NULL result=%d", (int)result);
        return;
    }
    if (httpFlowIsTerminalState(flow)) {
        LOG_TRACE("HTTP FLOW httpFlowFail(): ignored terminal=%s result=%d",
                  httpStateName(flow->state), (int)result);
        return;
    }
    LOG_ERROR("HTTP FLOW FAILED: result=%d state=%s nthResult=%d "
              "parseResult=%d httpStatus=%d",
              (int)result, httpStateName(flow->state),
              (int)flow->status.nthResult, (int)flow->status.parseResult,
              (int)flow->status.httpStatus);
    flow->status.result = result;
    httpFlowSetState(flow, HTTP_FLOW_FAILED);
    if (flow->callbacks.onDone != NULL) {
        LOG_TRACE("HTTP FLOW httpFlowFail(): notifying onDone");
        flow->callbacks.onDone(flow, &flow->status);
        /* Callback may release or reuse flow. Do not access flow afterward. */
    } else {
        LOG_TRACE("HTTP FLOW httpFlowFail(): no onDone callback");
    }
}

static void httpFlowComplete(HttpFlow* flow) {
    if (flow == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowComplete(): flow=NULL");
        return;
    }
    if (httpFlowIsTerminalState(flow)) {
        LOG_TRACE("HTTP FLOW httpFlowComplete(): already terminal=%s",
                  httpStateName(flow->state));
        return;
    }
    LOG_TRACE("HTTP FLOW COMPLETE: status=%d nthResult=%d parseResult=%d",
              (int)flow->status.httpStatus, (int)flow->status.nthResult,
              (int)flow->status.parseResult);
    flow->status.result = HTTP_FLOW_OK;
    httpFlowSetState(flow, HTTP_FLOW_COMPLETED);
    if (flow->callbacks.onDone != NULL) {
        LOG_TRACE("HTTP FLOW httpFlowComplete(): notifying onDone");
        flow->callbacks.onDone(flow, &flow->status);
        /* Callback may release or reuse flow. Do not access flow afterward. */
    } else {
        LOG_TRACE("HTTP FLOW httpFlowComplete(): no onDone callback");
    }
}

/* -----------------------------------------------------------
 * HTTP request construction
 * ----------------------------------------------------------- */
static bool httpIsValidHeaderName(const char* name) {
    size_t i;
    if (name == NULL || name[0] == '\0') {
        LOG_ERROR("HTTP FLOW header name: NULL or empty");
        return false;
    }
    for (i = 0u; name[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!isAlphaNumeric(c) && c != '-' && c != '_') {
            LOG_ERROR("HTTP FLOW header name: invalid character at index=%lu "
                      "code=0x%02X",
                      (unsigned long)i, (unsigned int)c);
            return false;
        }
    }
    return true;
}

static bool httpIsValidHeaderValue(const char* value) {
    size_t i;
    if (value == NULL) {
        LOG_ERROR("HTTP FLOW header value: NULL");
        return false;
    }
    for (i = 0u; value[i] != '\0'; ++i) {
        if (value[i] == '\r' || value[i] == '\n') {
            LOG_ERROR("HTTP FLOW header value: CR/LF rejected at index=%lu",
                      (unsigned long)i);
            return false;
        }
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

static bool httpAppend(ByteArray* buffer, const char* str) {
    size_t len;
    if (buffer == NULL || str == NULL || buffer->data == NULL) {
        LOG_ERROR("HTTP FLOW httpAppend(): invalid buffer/string");
        return false;
    }
    len = strlen(str);
    if (buffer->len > buffer->capacity ||
        len > buffer->capacity - buffer->len) {
        LOG_ERROR("HTTP FLOW httpAppend(): TX overflow used=%lu append=%lu "
                  "capacity=%lu",
                  (unsigned long)buffer->len, (unsigned long)len,
                  (unsigned long)buffer->capacity);
        return false;
    }
    memcpy(buffer->data + buffer->len, str, len);
    buffer->len += len;
    return true;
}

static bool httpAppendHeader(ByteArray* buffer, const char* name,
                             const char* value) {
    if (!httpIsValidHeaderName(name) || !httpIsValidHeaderValue(value)) {
        LOG_ERROR("HTTP FLOW httpAppendHeader(): invalid name/value");
        return false;
    }
    if (!httpAppend(buffer, name) || !httpAppend(buffer, ": ") ||
        !httpAppend(buffer, value) || !httpAppend(buffer, "\r\n")) {
        LOG_ERROR("HTTP FLOW httpAppendHeader(): failed to append header");
        return false;
    }
    return true;
}

static HttpFlowResult httpBuildRequest(HttpFlow* flow) {
    const HttpFlowRequest* req;
    ByteArray*             buffer;
    const char*            method;
    char                   line[320];
    int                    written;
    size_t                 i;

    if (flow == NULL) {
        LOG_ERROR("HTTP FLOW httpBuildRequest(): flow=NULL");
        return HTTP_FLOW_ERR_INVALID_ARG;
    }
    req    = &flow->request;
    buffer = &flow->requestBuffer;
    method = httpMethodString(req->method);
    if (method == NULL || req->path == NULL || req->host == NULL) {
        LOG_ERROR("HTTP FLOW httpBuildRequest(): method/path/host invalid");
        return HTTP_FLOW_ERR_REQUEST;
    }
    if ((req->headerCount != 0u && req->headers == NULL) ||
        (req->bodyLength != 0u && req->body == NULL)) {
        LOG_ERROR("HTTP FLOW httpBuildRequest(): headers/body pointer missing "
                  "headers=%lu bodyLen=%lu",
                  (unsigned long)req->headerCount,
                  (unsigned long)req->bodyLength);
        return HTTP_FLOW_ERR_REQUEST;
    }
    LOG_TRACE("HTTP FLOW httpBuildRequest(): %s %s port=%u headers=%lu "
              "bodyLen=%lu txCapacity=%lu",
              method, req->path, (unsigned int)req->port,
              (unsigned long)req->headerCount, (unsigned long)req->bodyLength,
              (unsigned long)buffer->capacity);

    written =
        snprintf(line, sizeof(line), "%s %s HTTP/1.1\r\n", method, req->path);
    if (written < 0 || (size_t)written >= sizeof(line) ||
        !httpAppend(buffer, line)) {
        LOG_ERROR("HTTP FLOW httpBuildRequest(): request line too long/append "
                  "failed result=%d",
                  written);
        return HTTP_FLOW_ERR_REQUEST;
    }

    /* Mandatory Host header for HTTP/1.1. */
    {
        char hostValue[HTTP_MAX_HEADER_VALUE_LEN + 1U];
        if (req->port == 80U)
            written = snprintf(hostValue, sizeof(hostValue), "%s", req->host);
        else
            written = snprintf(hostValue, sizeof(hostValue), "%s:%u", req->host,
                               (unsigned int)req->port);
        if (written < 0 || (size_t)written >= sizeof(hostValue) ||
            !httpAppendHeader(buffer, "Host", hostValue)) {
            LOG_ERROR(
                "HTTP FLOW httpBuildRequest(): Host header failed result=%d",
                written);
            return HTTP_FLOW_ERR_REQUEST;
        }
    }

    for (i = 0u; i < req->headerCount; ++i) {
        if (!httpAppendHeader(buffer, req->headers[i].name,
                              req->headers[i].value)) {
            LOG_ERROR("HTTP FLOW httpBuildRequest(): header index=%lu failed",
                      (unsigned long)i);
            return HTTP_FLOW_ERR_REQUEST;
        }
    }

    if (req->bodyLength > 0u) {
        char lenBuffer[32];
        written = snprintf(lenBuffer, sizeof(lenBuffer), "%lu",
                           (unsigned long)req->bodyLength);
        if (written < 0 || (size_t)written >= sizeof(lenBuffer) ||
            !httpAppendHeader(buffer, "Content-Length", lenBuffer)) {
            LOG_ERROR("HTTP FLOW httpBuildRequest(): Content-Length failed");
            return HTTP_FLOW_ERR_REQUEST;
        }
    }

    if (!httpAppend(buffer, "\r\n")) {
        LOG_ERROR("HTTP FLOW httpBuildRequest(): header terminator overflow");
        return HTTP_FLOW_ERR_REQUEST;
    }
    if (req->bodyLength > 0u) {
        if (buffer->len > buffer->capacity ||
            req->bodyLength > buffer->capacity - buffer->len) {
            LOG_ERROR("HTTP FLOW httpBuildRequest(): body overflow used=%lu "
                      "bodyLen=%lu capacity=%lu",
                      (unsigned long)buffer->len,
                      (unsigned long)req->bodyLength,
                      (unsigned long)buffer->capacity);
            return HTTP_FLOW_ERR_REQUEST;
        }
        memcpy(buffer->data + buffer->len, req->body, req->bodyLength);
        buffer->len += req->bodyLength;
    }
    LOG_TRACE("HTTP FLOW httpBuildRequest(): success txLen=%lu capacity=%lu",
              (unsigned long)buffer->len, (unsigned long)buffer->capacity);
    return HTTP_FLOW_OK;
}

/* -----------------------------------------------------------
 * Public initialization / start
 * ----------------------------------------------------------- */
void httpFlowInit(HttpFlow* flow) {
    if (flow == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowInit(): flow=NULL");
        return;
    }
    memset(flow, 0, sizeof(*flow));
    flow->state = HTTP_FLOW_IDLE;
    httpParserInit(&flow->parser);
    flow->status.result = HTTP_FLOW_OK;
    LOG_TRACE("HTTP FLOW httpFlowInit(): initialized state=IDLE");
}

HttpFlowResult httpFlowStart(HttpFlow* flow, State* owner, const char* host,
                             uint16_t port, const HttpFlowRequest* request,
                             const HttpFlowCallbacks* callbacks,
                             void*                    userData) {
    HttpFlowResult buildResult;
    NthResult      nthResult;

    LOG_TRACE("HTTP FLOW httpFlowStart(): begin port=%u flowSet=%d "
              "requestSet=%d hostSet=%d",
              (unsigned int)port, (int)(flow != NULL), (int)(request != NULL),
              (int)(host != NULL));

    if (flow == NULL || request == NULL || host == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowStart(): invalid arguments");
        return HTTP_FLOW_ERR_INVALID_ARG;
    }
    if (flow->state != HTTP_FLOW_IDLE) {
        LOG_ERROR("HTTP FLOW httpFlowStart(): BUSY state=%s(%d)",
                  httpStateName(flow->state), (int)flow->state);
        return HTTP_FLOW_ERR_BUSY;
    }
    if (!httpIsValidMethod(request->method)) {
        LOG_ERROR("HTTP FLOW httpFlowStart(): unsupported method=%d",
                  (int)request->method);
        return HTTP_FLOW_ERR_REQUEST;
    }
    if ((request->headerCount > 0u && request->headers == NULL) ||
        (request->bodyLength > 0u && request->body == NULL)) {
        LOG_ERROR("HTTP FLOW httpFlowStart(): headers/body pointer invalid");
        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    memcpy(&flow->request, request, sizeof(HttpFlowRequest));
    flow->request.host = host;
    flow->request.port = port;
    memset(&flow->status, 0, sizeof(flow->status));
    memset(&flow->callbacks, 0, sizeof(flow->callbacks));
    flow->status.result = HTTP_FLOW_OK;
    httpParserInit(&flow->parser);
    if (callbacks != NULL)
        memcpy(&flow->callbacks, callbacks, sizeof(flow->callbacks));
    flow->owner    = owner;
    flow->userData = userData;

    LOG_TRACE("HTTP FLOW httpFlowStart(): NTH alloc, method=%s path=%s "
              "headers=%lu bodyLen=%lu",
              httpMethodString(request->method),
              request->path != NULL ? request->path : "(null)",
              (unsigned long)request->headerCount,
              (unsigned long)request->bodyLength);

    flow->tx = nth()->alloc();
    if (flow->tx == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowStart(): NTH allocation failed");
        return HTTP_FLOW_ERR_NO_RESOURCE;
    }
    flow->ownsTransaction = true;
    flow->tx->owner       = owner;
    flow->tx->userData    = flow;
    flow->tx->onConnect   = httpFlowOnConnect;
    flow->tx->onSent      = httpFlowOnSent;
    flow->tx->isComplete  = httpFlowNthIsComplete;
    flow->tx->onReceive   = httpFlowNthReceive;
    flow->tx->onChunk     = NULL;
    flow->tx->onFailure   = httpFlowOnFailure;
    flow->tx->onTimeout   = httpFlowOnTimeout;

    flow->requestBuffer.data     = flow->tx->txBuffer.data;
    flow->requestBuffer.capacity = flow->tx->txBuffer.capacity;
    flow->requestBuffer.len      = 0u;
    httpFlowSetState(flow, HTTP_FLOW_BUILDING);

    buildResult = httpBuildRequest(flow);
    if (buildResult != HTTP_FLOW_OK) {
        LOG_ERROR("HTTP FLOW httpFlowStart(): request build failed result=%d",
                  (int)buildResult);
        httpFlowFail(flow, buildResult);
        return buildResult;
    }

    flow->tx->txBuffer.len = flow->requestBuffer.len;
    LOG_TRACE("HTTP FLOW httpFlowStart(): TX prepared bytes=%lu",
              (unsigned long)flow->tx->txBuffer.len);
    httpFlowSetState(flow, HTTP_FLOW_CONNECTING);
    httpNotifyState(flow, HTTP_FLOW_CONNECTING);

    LOG_TRACE("HTTP FLOW httpFlowStart(): NTH connecting host=%s port=%u", host,
              (unsigned int)port);
    nthResult = nth()->connect(flow->tx, host, port);
    if (nthResult != NTH_OK) {
        LOG_ERROR("HTTP FLOW httpFlowStart(): NTH connect failed nthResult=%d",
                  (int)nthResult);
        flow->status.nthResult = nthResult;
        httpFlowFail(flow, HTTP_FLOW_ERR_CONNECT);
        return HTTP_FLOW_ERR_CONNECT;
    }
    LOG_TRACE("HTTP FLOW httpFlowStart(): connect request accepted");
    return HTTP_FLOW_OK;
}

/* -----------------------------------------------------------
 * NTH event handlers
 * ----------------------------------------------------------- */
static int8_t httpFlowOnConnect(NthTransaction* tx, void* userData) {
    HttpFlow* flow;
    NthResult result;
    if (tx == NULL || userData == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowOnConnect(): invalid callback arguments");
        return -1;
    }
    flow = (HttpFlow*)userData;
    LOG_TRACE("HTTP FLOW httpFlowOnConnect(): connected, txLen=%lu",
              (unsigned long)tx->txBuffer.len);
    httpFlowSetState(flow, HTTP_FLOW_SENDING);
    httpNotifyState(flow, HTTP_FLOW_SENDING);

    /* Dump exactly what the NTH transport is about to transmit. */
    httpTraceWire("TX", tx->txBuffer.data, tx->txBuffer.len);

    result = nth()->sendProvidedTx(tx);
    if (result != NTH_OK) {
        LOG_ERROR("HTTP FLOW httpFlowOnConnect(): send failed nthResult=%d",
                  (int)result);
        flow->status.nthResult = result;
        httpFlowFail(flow, HTTP_FLOW_ERR_SEND);
        return -1;
    }
    LOG_TRACE("HTTP FLOW httpFlowOnConnect(): send request accepted");
    return 0;
}

static int8_t httpFlowOnSent(NthTransaction* tx, void* userData) {
    HttpFlow* flow;
    if (tx == NULL || userData == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowOnSent(): invalid callback arguments");
        return -1;
    }
    flow = (HttpFlow*)userData;
    LOG_TRACE("HTTP FLOW httpFlowOnSent(): sent bytes=%lu, awaiting response",
              (unsigned long)tx->txBuffer.len);
    httpFlowSetState(flow, HTTP_FLOW_RECEIVING_HEADERS);
    httpNotifyState(flow, HTTP_FLOW_RECEIVING_HEADERS);
    httpParserInit(&flow->parser);
    return 0;
}

static NthRxDecision httpFlowNthIsComplete(NthTransaction* tx, void* userData) {
    HttpFlow*       flow;
    HttpParseResult parseResult;
    bool            headerComplete;
    bool            messageComplete;

    if (tx == NULL || userData == NULL) {
        LOG_ERROR(
            "HTTP FLOW httpFlowNthIsComplete(): invalid callback arguments");
        return NTH_RX_ERROR;
    }
    flow = (HttpFlow*)userData;
    if (httpFlowIsTerminalState(flow)) {
        LOG_TRACE("HTTP FLOW httpFlowNthIsComplete(): terminal state=%s",
                  httpStateName(flow->state));
        return flow->state == HTTP_FLOW_COMPLETED ? NTH_RX_COMPLETE
                                                  : NTH_RX_ERROR;
    }
    LOG_TRACE("HTTP FLOW RX check: buffered=%lu peerClosed=%d state=%s",
              (unsigned long)tx->rxBuffer.len, (int)tx->peerClosed,
              httpStateName(flow->state));
    if (tx->peerClosed) {
        parseResult = httpParserFinish(&flow->parser);
    } else {
        parseResult = httpParserParseResponse(&flow->parser, tx->rxBuffer.data,
                                              tx->rxBuffer.len);
    }
    flow->status.parseResult = parseResult;
    headerComplete           = httpParserIsHeaderComplete(&flow->parser);
    messageComplete          = httpParserIsMessageComplete(&flow->parser);

    if (headerComplete) {
        const HttpResponse* response = httpParserGetResponse(&flow->parser);
        if (response != NULL)
            flow->status.httpStatus = response->statusCode;
        if (!messageComplete && flow->state != HTTP_FLOW_RECEIVING_BODY) {
            LOG_TRACE("HTTP FLOW RX headers complete: status=%d, awaiting body",
                      (int)flow->status.httpStatus);
            httpFlowSetState(flow, HTTP_FLOW_RECEIVING_BODY);
            httpNotifyState(flow, HTTP_FLOW_RECEIVING_BODY);
        }
    }

    LOG_TRACE("HTTP FLOW RX parse: %s(%d) headerComplete=%d messageComplete=%d "
              "status=%d",
              httpParseResultName(parseResult), (int)parseResult,
              (int)headerComplete, (int)messageComplete,
              (int)flow->status.httpStatus);
    switch (parseResult) {
    case HTTP_PARSE_OK:
        return messageComplete ? NTH_RX_COMPLETE : NTH_RX_MORE;
    case HTTP_PARSE_INCOMPLETE:
        if (tx->peerClosed)
            LOG_ERROR(
                "HTTP FLOW RX: peer closed before complete HTTP response");
        return tx->peerClosed ? NTH_RX_ERROR : NTH_RX_MORE;
    case HTTP_PARSE_ERROR:
    case HTTP_PARSE_OVERFLOW:
    case HTTP_PARSE_UNSUPPORTED:
    default:
        LOG_ERROR(
            "HTTP FLOW RX: parser rejected response result=%s(%d) buffered=%lu",
            httpParseResultName(parseResult), (int)parseResult,
            (unsigned long)tx->rxBuffer.len);
        return NTH_RX_ERROR;
    }
}

static int8_t httpFlowNthReceive(NthTransaction* tx, void* userData) {
    HttpFlow*           flow;
    const HttpResponse* response;
    const uint8_t*      body;
    size_t              bodyLength;

    if (tx == NULL || userData == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowNthReceive(): invalid callback arguments");
        return -1;
    }
    flow = (HttpFlow*)userData;
    if (httpFlowIsTerminalState(flow)) {
        LOG_TRACE("HTTP FLOW httpFlowNthReceive(): ignored terminal=%s",
                  httpStateName(flow->state));
        return 0;
    }

    /* Full accumulated wire message is available here, once per response. */
    httpTraceWire("RX", tx->rxBuffer.data, tx->rxBuffer.len);

    if (!httpParserIsMessageComplete(&flow->parser)) {
        LOG_ERROR(
            "HTTP FLOW httpFlowNthReceive(): parser did not complete message");
        httpFlowFail(flow, HTTP_FLOW_ERR_PARSE);
        return -1;
    }
    response = httpParserGetResponse(&flow->parser);
    if (response == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowNthReceive(): parsed response=NULL");
        httpFlowFail(flow, HTTP_FLOW_ERR_PARSE);
        return -1;
    }
    flow->status.httpStatus = response->statusCode;
    LOG_TRACE("HTTP FLOW httpFlowNthReceive(): response status=%d totalRx=%lu",
              (int)response->statusCode, (unsigned long)tx->rxBuffer.len);
    if (flow->callbacks.onHeaders != NULL) {
        LOG_TRACE("HTTP FLOW httpFlowNthReceive(): invoking onHeaders");
        flow->callbacks.onHeaders(flow, response);
    }
    if (httpFlowIsTerminalState(flow)) {
        LOG_TRACE(
            "HTTP FLOW httpFlowNthReceive(): stopped by onHeaders callback");
        return 0;
    }

    body =
        httpParserGetBody(&flow->parser, tx->rxBuffer.data, tx->rxBuffer.len);
    bodyLength = httpParserGetBodyLength(&flow->parser, tx->rxBuffer.len);
    LOG_TRACE("HTTP FLOW httpFlowNthReceive(): bodyPresent=%d bodyLength=%lu "
              "onBodySet=%d",
              (int)(body != NULL), (unsigned long)bodyLength,
              (int)(flow->callbacks.onBody != NULL));
    if (body != NULL && bodyLength > 0u && flow->callbacks.onBody != NULL) {
        httpFlowSetState(flow, HTTP_FLOW_RECEIVING_BODY);
        httpNotifyState(flow, HTTP_FLOW_RECEIVING_BODY);
        LOG_TRACE("HTTP FLOW httpFlowNthReceive(): invoking onBody len=%lu",
                  (unsigned long)bodyLength);
        if (flow->callbacks.onBody(flow, body, bodyLength) != 0) {
            LOG_ERROR(
                "HTTP FLOW httpFlowNthReceive(): onBody rejected payload");
            httpFlowFail(flow, HTTP_FLOW_ERR_BODY);
            return -1;
        }
    }
    if (httpFlowIsTerminalState(flow))
        return 0;

    /* Preserve existing behavior: 2xx (including 206) succeeds. */
    if (response->statusCode < 200 || response->statusCode >= 300) {
        LOG_ERROR("HTTP FLOW httpFlowNthReceive(): HTTP status error=%d",
                  (int)response->statusCode);
        httpFlowFail(flow, HTTP_FLOW_ERR_HTTP_STATUS);
        return 0;
    }
    httpFlowComplete(flow);
    return 0;
}

static int8_t httpFlowOnFailure(NthTransaction* tx, void* userData) {
    HttpFlow*      flow;
    HttpFlowResult result = HTTP_FLOW_ERR_RECEIVE;
    if (userData == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowOnFailure(): flow context=NULL");
        return -1;
    }
    flow = (HttpFlow*)userData;
    if (httpFlowIsTerminalState(flow)) {
        LOG_TRACE("HTTP FLOW httpFlowOnFailure(): ignored terminal=%s",
                  httpStateName(flow->state));
        return 0;
    }
    if (tx != NULL) {
        flow->status.nthResult = tx->lastError;
        LOG_ERROR(
            "HTTP FLOW httpFlowOnFailure(): nthError=%d rxLen=%lu state=%s",
            (int)tx->lastError, (unsigned long)tx->rxBuffer.len,
            httpStateName(flow->state));
        if (tx->rxBuffer.len > 0u)
            httpTraceWire("RX-PARTIAL", tx->rxBuffer.data, tx->rxBuffer.len);
    } else {
        LOG_ERROR("HTTP FLOW httpFlowOnFailure(): transaction=NULL");
    }
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
    LOG_ERROR("HTTP FLOW httpFlowOnFailure(): mapped result=%d parse=%s",
              (int)result, httpParseResultName(flow->status.parseResult));
    httpFlowFail(flow, result);
    return 0;
}

static int8_t httpFlowOnTimeout(NthTransaction* tx, void* userData) {
    HttpFlow* flow;
    if (userData == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowOnTimeout(): flow context=NULL");
        return -1;
    }
    flow = (HttpFlow*)userData;
    if (httpFlowIsTerminalState(flow))
        return 0;
    if (tx != NULL) {
        flow->status.nthResult = tx->lastError;
        LOG_ERROR("HTTP FLOW httpFlowOnTimeout(): timeout state=%s nthError=%d "
                  "rxLen=%lu",
                  httpStateName(flow->state), (int)tx->lastError,
                  (unsigned long)tx->rxBuffer.len);
        if (tx->rxBuffer.len > 0u)
            httpTraceWire("RX-TIMEOUT", tx->rxBuffer.data, tx->rxBuffer.len);
    } else {
        LOG_ERROR("HTTP FLOW httpFlowOnTimeout(): timeout state=%s tx=NULL",
                  httpStateName(flow->state));
    }
    httpFlowFail(flow, HTTP_FLOW_ERR_TIMEOUT);
    return 0;
}

/* -----------------------------------------------------------
 * Public lifecycle and accessors
 * ----------------------------------------------------------- */
void httpFlowCancel(HttpFlow* flow) {
    if (flow == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowCancel(): flow=NULL");
        return;
    }
    LOG_TRACE("HTTP FLOW httpFlowCancel(): state=%s ownsTx=%d txSet=%d",
              httpStateName(flow->state), (int)flow->ownsTransaction,
              (int)(flow->tx != NULL));
    if (flow->tx != NULL) {
        LOG_TRACE("HTTP FLOW httpFlowCancel(): disconnecting NTH");
        nth()->disconnect(flow->tx);
    }
    httpFlowFail(flow, HTTP_FLOW_ERR_INTERNAL);
}

void httpFlowRelease(HttpFlow* flow) {
    if (flow == NULL) {
        LOG_ERROR("HTTP FLOW httpFlowRelease(): flow=NULL");
        return;
    }
    LOG_TRACE("HTTP FLOW httpFlowRelease(): state=%s txSet=%d ownsTx=%d",
              httpStateName(flow->state), (int)(flow->tx != NULL),
              (int)flow->ownsTransaction);
    if (flow->tx != NULL && flow->ownsTransaction) {
        LOG_TRACE("HTTP FLOW httpFlowRelease(): releasing NTH transaction");
        nth()->release(flow->tx);
    }
    flow->tx                     = NULL;
    flow->ownsTransaction        = false;
    flow->requestBuffer.data     = NULL;
    flow->requestBuffer.len      = 0u;
    flow->requestBuffer.capacity = 0u;
    flow->state                  = HTTP_FLOW_IDLE;
    LOG_TRACE("HTTP FLOW httpFlowRelease(): released, state=IDLE");
}

bool httpFlowIsComplete(const HttpFlow* flow) {
    return flow != NULL && flow->state == HTTP_FLOW_COMPLETED;
}

bool httpFlowIsFailed(const HttpFlow* flow) {
    return flow != NULL && flow->state == HTTP_FLOW_FAILED;
}

const HttpResponse* httpFlowGetResponse(const HttpFlow* flow) {
    if (flow == NULL || !flow->parser.headerComplete)
        return NULL;
    return &flow->parser.response;
}

const uint8_t* httpFlowGetBody(const HttpFlow* flow) {
    if (flow == NULL || flow->tx == NULL)
        return NULL;
    return httpParserGetBody(&flow->parser, flow->tx->rxBuffer.data,
                             flow->tx->rxBuffer.len);
}

size_t httpFlowGetBodyLength(const HttpFlow* flow) {
    if (flow == NULL || flow->tx == NULL)
        return 0u;
    return httpParserGetBodyLength(&flow->parser, flow->tx->rxBuffer.len);
}
