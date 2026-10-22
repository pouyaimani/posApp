#include "httpClient.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "httpParserCfg.h"
#include "logger.h"

/*
 * Wire logging is intended for controlled debugging only.
 * HTTP bodies can contain credentials, identifiers, and other sensitive data.
 * Disable these switches in production builds.
 */
#ifndef HTTP_CLIENT_TRACE_WIRE
#define HTTP_CLIENT_TRACE_WIRE 1
#endif
#ifndef HTTP_CLIENT_TRACE_BODY
#define HTTP_CLIENT_TRACE_BODY 1
#endif
#ifndef HTTP_CLIENT_TRACE_MAX_BYTES
#define HTTP_CLIENT_TRACE_MAX_BYTES 2048u
#endif

#define HTTP_CLIENT_LOG_LINE_LEN  120u
#define HTTP_CLIENT_LOG_HEX_WIDTH 16u

static bool   httpNthIsComplete(NthTransaction* tx, void* userData);
static int8_t httpNthReceive(NthTransaction* tx, void* userData);
static int8_t httpNthFailure(NthTransaction* tx, void* userData);

/* -----------------------------------------------------------
 * Logging helpers
 * ----------------------------------------------------------- */

#define HTTP_CLIENT_CHANGE_STATE(client, nextState)                            \
    do {                                                                       \
        if ((client)->state != (nextState)) {                                  \
            LOG_TRACE("HTTP CLIENT state: %d -> %d", (int)(client)->state,     \
                      (int)(nextState));                                       \
            (client)->state = (nextState);                                     \
        }                                                                      \
    } while (0)

#if HTTP_CLIENT_TRACE_WIRE
static bool httpClientHeaderNameEquals(const uint8_t* name, size_t nameLen,
                                       const char* expected) {
    size_t i;
    size_t expectedLen = strlen(expected);
    if (nameLen != expectedLen)
        return false;
    for (i = 0; i < nameLen; ++i) {
        if (tolower((unsigned char)name[i]) !=
            tolower((unsigned char)expected[i]))
            return false;
    }
    return true;
}

static bool httpClientIsSensitiveHeader(const uint8_t* line, size_t length) {
    size_t i;
    for (i = 0; i < length && line[i] != ':'; ++i) {
    }
    if (i == length)
        return false;
    return httpClientHeaderNameEquals(line, i, "Authorization") ||
           httpClientHeaderNameEquals(line, i, "Proxy-Authorization") ||
           httpClientHeaderNameEquals(line, i, "Cookie") ||
           httpClientHeaderNameEquals(line, i, "Set-Cookie") ||
           httpClientHeaderNameEquals(line, i, "X-API-Key") ||
           httpClientHeaderNameEquals(line, i, "X-Auth-Token");
}

/* Log one header line without assuming the wire buffer is null-terminated. */
static void httpClientTraceHeaderLine(const char*    direction,
                                      const uint8_t* line, size_t length) {
    char   text[HTTP_CLIENT_LOG_LINE_LEN + 1u];
    size_t i, count;

    if (httpClientIsSensitiveHeader(line, length)) {
        LOG_TRACE("HTTP CLIENT %s HEADER: [sensitive header redacted]",
                  direction);
        return;
    }

    count =
        length < HTTP_CLIENT_LOG_LINE_LEN ? length : HTTP_CLIENT_LOG_LINE_LEN;
    for (i = 0; i < count; ++i) {
        unsigned char c = line[i];
        text[i]         = isprint(c) ? (char)c : '.';
    }
    text[count] = '\0';
    LOG_TRACE("HTTP CLIENT %s HEADER: %s%s", direction, text,
              length > count ? " [truncated]" : "");
}

#if HTTP_CLIENT_TRACE_BODY
static bool httpClientBodyIsText(const uint8_t* data, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) {
        unsigned char c = data[i];
        if (c != '\r' && c != '\n' && c != '\t' && !isprint(c))
            return false;
    }
    return true;
}

/* Print the body in bounded lines; hexadecimal is used for binary payloads. */
static void httpClientTraceBody(const char* direction, const uint8_t* data,
                                size_t length) {
    size_t offset = 0u;
    bool   textBody;

    if (data == NULL || length == 0u)
        return;

    textBody = httpClientBodyIsText(data, length);
    LOG_TRACE("HTTP CLIENT %s BODY: format=%s printedBytes=%lu", direction,
              textBody ? "text" : "hex", (unsigned long)length);

    while (offset < length) {
        char   line[HTTP_CLIENT_LOG_LINE_LEN + 1u];
        size_t i, count;

        if (textBody) {
            count = length - offset;
            if (count > HTTP_CLIENT_LOG_LINE_LEN)
                count = HTTP_CLIENT_LOG_LINE_LEN;
            for (i = 0; i < count; ++i) {
                unsigned char c = data[offset + i];
                /* One log record per chunk, even if body contains CR/LF. */
                line[i] = (c == '\r' || c == '\n' || c == '\t') ? ' ' : (char)c;
            }
            line[count] = '\0';
            LOG_TRACE("HTTP CLIENT %s BODY[%lu]: %s", direction,
                      (unsigned long)offset, line);
        } else {
            size_t used = 0u;
            count       = length - offset;
            if (count > HTTP_CLIENT_LOG_HEX_WIDTH)
                count = HTTP_CLIENT_LOG_HEX_WIDTH;
            for (i = 0; i < count; ++i) {
                int written = snprintf(line + used, sizeof(line) - used,
                                       "%02X ", (unsigned int)data[offset + i]);
                if (written < 0 || (size_t)written >= sizeof(line) - used)
                    break;
                used += (size_t)written;
            }
            line[used] = '\0';
            LOG_TRACE("HTTP CLIENT %s BODY HEX[%lu]: %s", direction,
                      (unsigned long)offset, line);
        }
        offset += count;
    }
}

#endif /* HTTP_CLIENT_TRACE_BODY */
#endif /* HTTP_CLIENT_TRACE_WIRE */

/* Dump a bounded HTTP message. No read relies on a terminating '\0'. */
static void httpClientTraceWire(const char* direction, const uint8_t* data,
                                size_t length) {
#if HTTP_CLIENT_TRACE_WIRE
    size_t limit, pos = 0u, bodyOffset = 0u;
    bool   headerDone = false;

    if (data == NULL) {
        LOG_ERROR("HTTP CLIENT %s WIRE: data is NULL, length=%lu", direction,
                  (unsigned long)length);
        return;
    }
    limit = length < HTTP_CLIENT_TRACE_MAX_BYTES ? length
                                                 : HTTP_CLIENT_TRACE_MAX_BYTES;
    LOG_TRACE("HTTP CLIENT %s WIRE: totalBytes=%lu traceBytes=%lu", direction,
              (unsigned long)length, (unsigned long)limit);

    /* Scan only the logged portion of the message for CRLF-delimited lines. */
    while (pos < limit) {
        size_t end = pos;
        while (end + 1u < limit &&
               !(data[end] == '\r' && data[end + 1u] == '\n'))
            ++end;
        if (end + 1u >= limit)
            break;

        if (end == pos) { /* Blank line separates HTTP headers and body. */
            bodyOffset = end + 2u;
            headerDone = true;
            break;
        }

        httpClientTraceHeaderLine(direction, data + pos, end - pos);
        pos = end + 2u;
    }

    if (!headerDone) {
        LOG_TRACE(
            "HTTP CLIENT %s WIRE: headers incomplete or trace limit reached",
            direction);
    } else {
        size_t bodyTotal   = length - bodyOffset;
        size_t bodyVisible = limit - bodyOffset;
        LOG_TRACE("HTTP CLIENT %s WIRE: headerBytes=%lu bodyBytes=%lu",
                  direction, (unsigned long)bodyOffset,
                  (unsigned long)bodyTotal);
#if HTTP_CLIENT_TRACE_BODY
        httpClientTraceBody(direction, data + bodyOffset, bodyVisible);
#else
        (void)bodyVisible;
        LOG_TRACE("HTTP CLIENT %s WIRE: body logging disabled", direction);
#endif
    }

    if (length > limit)
        LOG_TRACE("HTTP CLIENT %s WIRE: truncated %lu additional bytes",
                  direction, (unsigned long)(length - limit));
#else
    (void)direction;
    (void)data;
    (void)length;
#endif
}

static void httpClientReset(HttpClient* client) {
    if (client == NULL) {
        LOG_ERROR("HTTP CLIENT httpClientReset(): client is NULL");
        return;
    }
    LOG_TRACE("HTTP CLIENT httpClientReset(): oldState=%d", (int)client->state);
    httpParserInit(&client->parser);
    HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_IDLE);
    client->parseResult = HTTP_PARSE_INCOMPLETE;
    client->nthResult   = NTH_OK;
    LOG_TRACE("HTTP CLIENT httpClientReset(): parser and results reset");
}

void httpClientInit(HttpClient* client, NthTransaction* tx, void* userData) {
    LOG_TRACE("HTTP CLIENT httpClientInit(): txSet=%d userDataSet=%d",
              (int)(tx != NULL), (int)(userData != NULL));
    if (client == NULL) {
        LOG_ERROR("HTTP CLIENT httpClientInit(): client is NULL");
        return;
    }

    memset(client, 0, sizeof(*client));
    client->tx       = tx;
    client->userData = userData;
    httpClientReset(client);
    LOG_TRACE("HTTP CLIENT httpClientInit(): initialized successfully");
}

static HttpClientResult appendString(char* buffer, size_t capacity,
                                     size_t* offset, const char* str) {
    size_t len;
    if (buffer == NULL || offset == NULL || str == NULL) {
        LOG_ERROR("HTTP CLIENT appendString(): invalid argument");
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }

    len = strlen(str);
    if (*offset > capacity || len > capacity - *offset) {
        LOG_ERROR("HTTP CLIENT appendString(): overflow offset=%lu add=%lu "
                  "capacity=%lu",
                  (unsigned long)*offset, (unsigned long)len,
                  (unsigned long)capacity);
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
    if (name == NULL || value == NULL) {
        LOG_ERROR("HTTP CLIENT appendHeader(): header name/value is NULL");
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }
    LOG_TRACE("HTTP CLIENT appendHeader(): adding header name=%s", name);
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
    ByteArray        request;
    size_t           offset = 0u;
    char             line[HTTP_MAX_TARGET_LEN + 64U];
    int              ret;
    HttpClientResult result;
    size_t           i;

    LOG_TRACE("HTTP CLIENT buildRequest(): method=%s path=%s host=%s "
              "headers=%lu bodyLen=%lu",
              method != NULL ? method : "(null)",
              path != NULL ? path : "(null)", host != NULL ? host : "(null)",
              (unsigned long)headerCount, (unsigned long)bodyLen);

    if (client == NULL || client->tx == NULL || method == NULL ||
        path == NULL || host == NULL) {
        LOG_ERROR("HTTP CLIENT buildRequest(): missing required argument");
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }
    if (headerCount > 0u && headers == NULL) {
        LOG_ERROR("HTTP CLIENT buildRequest(): headers is NULL but count=%lu",
                  (unsigned long)headerCount);
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }
    if (bodyLen > 0u && body == NULL) {
        LOG_ERROR("HTTP CLIENT buildRequest(): body is NULL but length=%lu",
                  (unsigned long)bodyLen);
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }
    if (bodyLen > HTTP_MAX_BODY_SIZE) {
        LOG_ERROR("HTTP CLIENT buildRequest(): body too large len=%lu max=%lu",
                  (unsigned long)bodyLen, (unsigned long)HTTP_MAX_BODY_SIZE);
        return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;
    }

    request.data     = client->tx->txBuffer.data;
    request.len      = 0u;
    request.capacity = client->tx->txBuffer.capacity;
    if (request.data == NULL || request.capacity == 0u) {
        LOG_ERROR("HTTP CLIENT buildRequest(): TX buffer missing/capacity=0");
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }

    ret = snprintf(line, sizeof(line), "%s %s HTTP/1.1\r\n", method, path);
    if (ret < 0 || (size_t)ret >= sizeof(line)) {
        LOG_ERROR("HTTP CLIENT buildRequest(): request line too long ret=%d "
                  "capacity=%lu",
                  ret, (unsigned long)sizeof(line));
        return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;
    }
    result = appendString((char*)request.data, request.capacity, &offset, line);
    if (result != HTTP_CLIENT_OK)
        return result;

    result = appendHeader((char*)request.data, request.capacity, &offset,
                          "Host", host);
    if (result != HTTP_CLIENT_OK)
        return result;

    for (i = 0u; i < headerCount; ++i) {
        if (headers[i].name == NULL || headers[i].value == NULL) {
            LOG_ERROR(
                "HTTP CLIENT buildRequest(): header[%lu] missing name/value",
                (unsigned long)i);
            return HTTP_CLIENT_ERR_INVALID_ARG;
        }
        result = appendHeader((char*)request.data, request.capacity, &offset,
                              headers[i].name, headers[i].value);
        if (result != HTTP_CLIENT_OK) {
            LOG_ERROR("HTTP CLIENT buildRequest(): header[%lu] append failed "
                      "result=%d",
                      (unsigned long)i, (int)result);
            return result;
        }
    }

    if (body != NULL && bodyLen > 0u) {
        char contentLength[32];
        ret = snprintf(contentLength, sizeof(contentLength), "%lu",
                       (unsigned long)bodyLen);
        if (ret < 0 || (size_t)ret >= sizeof(contentLength)) {
            LOG_ERROR(
                "HTTP CLIENT buildRequest(): Content-Length conversion failed");
            return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;
        }
        result = appendHeader((char*)request.data, request.capacity, &offset,
                              "Content-Length", contentLength);
        if (result != HTTP_CLIENT_OK)
            return result;
    }

    result =
        appendString((char*)request.data, request.capacity, &offset, "\r\n");
    if (result != HTTP_CLIENT_OK)
        return result;

    if (bodyLen > 0u) {
        if (offset > request.capacity || bodyLen > request.capacity - offset) {
            LOG_ERROR("HTTP CLIENT buildRequest(): TX capacity exceeded at "
                      "body offset=%lu body=%lu cap=%lu",
                      (unsigned long)offset, (unsigned long)bodyLen,
                      (unsigned long)request.capacity);
            return HTTP_CLIENT_ERR_REQUEST_TOO_LARGE;
        }
        memcpy(request.data + offset, body, bodyLen);
        offset += bodyLen;
    }

    request.len              = offset;
    client->tx->txBuffer.len = request.len;
    HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_REQUEST_READY);

    LOG_TRACE("HTTP CLIENT buildRequest(): success txLen=%lu txCapacity=%lu",
              (unsigned long)request.len, (unsigned long)request.capacity);
    httpClientTraceWire("TX", request.data, request.len);
    return HTTP_CLIENT_OK;
}

HttpClientResult httpClientBuildGet(HttpClient* client, const char* path,
                                    const char*              host,
                                    const HttpRequestHeader* headers,
                                    size_t                   headerCount) {
    HttpClientResult result;
    LOG_TRACE("HTTP CLIENT httpClientBuildGet(): preparing GET");
    result =
        buildRequest(client, "GET", path, host, headers, headerCount, NULL, 0u);
    LOG_TRACE("HTTP CLIENT httpClientBuildGet(): result=%d", (int)result);
    return result;
}

HttpClientResult httpClientBuildPost(HttpClient* client, const char* path,
                                     const char*              host,
                                     const HttpRequestHeader* headers,
                                     size_t headerCount, const uint8_t* body,
                                     size_t bodyLen) {
    HttpClientResult result;
    LOG_TRACE("HTTP CLIENT httpClientBuildPost(): preparing POST bodyLen=%lu",
              (unsigned long)bodyLen);
    result = buildRequest(client, "POST", path, host, headers, headerCount,
                          body, bodyLen);
    LOG_TRACE("HTTP CLIENT httpClientBuildPost(): result=%d", (int)result);
    return result;
}

HttpClientResult httpClientSend(HttpClient* client) {
    NthResult result;
    LOG_TRACE("HTTP CLIENT httpClientSend(): called clientSet=%d",
              (int)(client != NULL));
    if (client == NULL || client->tx == NULL) {
        LOG_ERROR("HTTP CLIENT httpClientSend(): client/transaction is NULL");
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }
    if (client->state != HTTP_CLIENT_REQUEST_READY) {
        LOG_ERROR("HTTP CLIENT httpClientSend(): invalid state=%d expected=%d",
                  (int)client->state, (int)HTTP_CLIENT_REQUEST_READY);
        return HTTP_CLIENT_ERR_INVALID_ARG;
    }

    LOG_TRACE("HTTP CLIENT httpClientSend(): txBytes=%lu resetting parser",
              (unsigned long)client->tx->txBuffer.len);
    httpParserInit(&client->parser);
    client->parseResult = HTTP_PARSE_INCOMPLETE;
    client->nthResult   = NTH_OK;

    client->tx->isComplete = httpNthIsComplete;
    client->tx->onReceive  = httpNthReceive;
    client->tx->onFailure  = httpNthFailure;
    client->tx->userData   = client;

    /* Set before the send: NTH may invoke callbacks synchronously. */
    HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_REQUESTING);
    LOG_TRACE("HTTP CLIENT httpClientSend(): calling sendProvidedTx()");
    result = nth()->sendProvidedTx(client->tx);

    if (result != NTH_OK) {
        LOG_ERROR("HTTP CLIENT httpClientSend(): NTH send failed result=%d",
                  (int)result);
        client->nthResult = result;
        HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_FAILED);
        return HTTP_CLIENT_ERR_NTH;
    }

    /* The synchronous receive callback may have released the client. */
    LOG_TRACE("HTTP CLIENT httpClientSend(): NTH accepted send result=%d",
              (int)result);
    return HTTP_CLIENT_OK;
}

const HttpResponse* httpClientGetResponse(const HttpClient* client) {
    if (client == NULL || !client->parser.headerComplete)
        return NULL;
    return &client->parser.response;
}

bool httpClientIsComplete(const HttpClient* client) {
    return client != NULL && client->state == HTTP_CLIENT_COMPLETED;
}

bool httpClientIsFailed(const HttpClient* client) {
    return client != NULL && client->state == HTTP_CLIENT_FAILED;
}

static bool httpNthIsComplete(NthTransaction* tx, void* userData) {
    HttpClient*     client = (HttpClient*)userData;
    HttpParseResult oldResult;
    bool            previousHeaders;
    bool            complete;

    if (tx == NULL || client == NULL) {
        LOG_ERROR(
            "HTTP CLIENT httpNthIsComplete(): invalid args tx=%d client=%d",
            (int)(tx != NULL), (int)(client != NULL));
        return false;
    }
    if (tx->rxBuffer.data == NULL && tx->rxBuffer.len != 0u) {
        LOG_ERROR("HTTP CLIENT httpNthIsComplete(): RX data is NULL len=%lu",
                  (unsigned long)tx->rxBuffer.len);
        client->parseResult = HTTP_PARSE_ERROR;
        client->nthResult   = NTH_ERR_INTERNAL;
        HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_FAILED);
        return true;
    }

    oldResult           = client->parseResult;
    previousHeaders     = httpParserIsHeaderComplete(&client->parser);
    client->parseResult = httpParserParseResponse(
        &client->parser, tx->rxBuffer.data, tx->rxBuffer.len);
    complete = httpParserIsMessageComplete(&client->parser);

    if (client->parseResult != oldResult ||
        (!previousHeaders && httpParserIsHeaderComplete(&client->parser))) {
        LOG_TRACE("HTTP CLIENT httpNthIsComplete(): rxLen=%lu parse=%d "
                  "headerComplete=%d messageComplete=%d",
                  (unsigned long)tx->rxBuffer.len, (int)client->parseResult,
                  (int)httpParserIsHeaderComplete(&client->parser),
                  (int)complete);
        if (!previousHeaders && httpParserIsHeaderComplete(&client->parser)) {
            const HttpResponse* response = httpClientGetResponse(client);
            LOG_TRACE(
                "HTTP CLIENT httpNthIsComplete(): headers parsed status=%d",
                response != NULL ? (int)response->statusCode : -1);
        }
    }

    switch (client->parseResult) {
    case HTTP_PARSE_OK:
        if (complete) {
            HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_COMPLETED);
            LOG_TRACE(
                "HTTP CLIENT httpNthIsComplete(): response complete rxLen=%lu",
                (unsigned long)tx->rxBuffer.len);
            httpClientTraceWire("RX", tx->rxBuffer.data, tx->rxBuffer.len);
            return true;
        }
        HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_RESPONSE_RECEIVING);
        return false;

    case HTTP_PARSE_INCOMPLETE:
        HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_RESPONSE_RECEIVING);
        return false;

    case HTTP_PARSE_OVERFLOW:
        LOG_ERROR("HTTP CLIENT httpNthIsComplete(): parser overflow rxLen=%lu",
                  (unsigned long)tx->rxBuffer.len);
        client->nthResult = NTH_ERR_OVERFLOW;
        HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_FAILED);
        httpClientTraceWire("RX-ERROR", tx->rxBuffer.data, tx->rxBuffer.len);
        return true;

    case HTTP_PARSE_ERROR:
    default:
        LOG_ERROR("HTTP CLIENT httpNthIsComplete(): parse failure result=%d "
                  "rxLen=%lu",
                  (int)client->parseResult, (unsigned long)tx->rxBuffer.len);
        client->nthResult = NTH_ERR_INTERNAL;
        HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_FAILED);
        httpClientTraceWire("RX-ERROR", tx->rxBuffer.data, tx->rxBuffer.len);
        return true;
    }
}

static int8_t httpNthReceive(NthTransaction* tx, void* userData) {
    HttpClient* client = (HttpClient*)userData;
    if (tx == NULL || client == NULL) {
        LOG_ERROR("HTTP CLIENT httpNthReceive(): invalid args tx=%d client=%d",
                  (int)(tx != NULL), (int)(client != NULL));
        return -1;
    }

    LOG_TRACE("HTTP CLIENT httpNthReceive(): state=%d parse=%d nthResult=%d "
              "rxLen=%lu",
              (int)client->state, (int)client->parseResult,
              (int)client->nthResult, (unsigned long)tx->rxBuffer.len);

    if (client->state != HTTP_CLIENT_COMPLETED) {
        LOG_TRACE(
            "HTTP CLIENT httpNthReceive(): ignored, response not complete");
        return 0;
    }

    if (client->onComplete != NULL) {
        LOG_TRACE("HTTP CLIENT httpNthReceive(): invoking onComplete");
        /* The owner may free/reinitialize client during this callback. */
        client->onComplete(client, client->userData);
    } else {
        LOG_TRACE(
            "HTTP CLIENT httpNthReceive(): onComplete callback not configured");
    }
    return 0;
}

static int8_t httpNthFailure(NthTransaction* tx, void* userData) {
    HttpClient* client = (HttpClient*)userData;
    if (client == NULL) {
        LOG_ERROR("HTTP CLIENT httpNthFailure(): client is NULL");
        return -1;
    }

    LOG_ERROR(
        "HTTP CLIENT httpNthFailure(): NTH failure state=%d txSet=%d parse=%d",
        (int)client->state, (int)(tx != NULL), (int)client->parseResult);
    if (tx != NULL) {
        client->nthResult = (NthResult)tx->lastError;
        LOG_ERROR("HTTP CLIENT httpNthFailure(): lastError=%d rxLen=%lu",
                  (int)client->nthResult, (unsigned long)tx->rxBuffer.len);
        if (tx->rxBuffer.data != NULL && tx->rxBuffer.len != 0u)
            httpClientTraceWire("RX-FAIL", tx->rxBuffer.data, tx->rxBuffer.len);
    }

    HTTP_CLIENT_CHANGE_STATE(client, HTTP_CLIENT_FAILED);
    if (client->onFailure != NULL) {
        LOG_TRACE("HTTP CLIENT httpNthFailure(): invoking onFailure");
        /* The owner may free/reinitialize client during this callback. */
        client->onFailure(client, client->userData);
    } else {
        LOG_TRACE(
            "HTTP CLIENT httpNthFailure(): onFailure callback not configured");
    }
    return 0;
}
