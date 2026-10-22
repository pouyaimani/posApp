#include "httpParser.h"
#include "logger.h"
#include "error.h"

#include <string.h>
#include <stdint.h>

#include "picohttpparser/picohttpparser.h"

/*
 * HTTP parser diagnostics.
 *
 * This parser receives an accumulated RX buffer on every invocation. Log the
 * headers and body ONCE per successfully decoded message, not on every poll.
 * Body logging is for development only: JSON may contain private device data.
 */
#ifndef HTTP_PARSER_TRACE_HEADERS
#define HTTP_PARSER_TRACE_HEADERS 1
#endif

#ifndef HTTP_PARSER_TRACE_BODY
#define HTTP_PARSER_TRACE_BODY 1
#endif

#ifndef HTTP_PARSER_TRACE_MAX_BYTES
#define HTTP_PARSER_TRACE_MAX_BYTES 512u
#endif

static const char* httpParseResultText(HttpParseResult result) {
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

static bool sliceEqualsIgnoreCase(HttpSlice slice, const char* str) {
    size_t len;

    if (str == NULL)
        return false;

    len = strlen(str);

    if (slice.len != len)
        return false;

    for (size_t i = 0; i < len; i++) {
        char a = slice.data[i];
        char b = str[i];

        if (a >= 'A' && a <= 'Z')
            a = (char)(a + ('a' - 'A'));

        if (b >= 'A' && b <= 'Z')
            b = (char)(b + ('a' - 'A'));

        if (a != b)
            return false;
    }

    return true;
}

#if HTTP_PARSER_TRACE_HEADERS
/* Escape non-printable bytes and honor explicit slice lengths. */
static void httpParserSafeText(char* out, size_t capacity, const char* data,
                               size_t length) {
    size_t n = length;
    if (out == NULL || capacity == 0u)
        return;
    if (data == NULL) {
        out[0] = '\0';
        return;
    }
    if (n >= capacity)
        n = capacity - 1u;
    for (size_t i = 0u; i < n; i++) {
        unsigned char c = (unsigned char)data[i];
        out[i]          = (c >= 32u && c <= 126u) ? (char)c : '.';
    }
    out[n] = '\0';
}
#endif

#if HTTP_PARSER_TRACE_HEADERS
static bool httpParserSensitiveHeader(HttpSlice name) {
    return sliceEqualsIgnoreCase(name, "Authorization") ||
           sliceEqualsIgnoreCase(name, "Proxy-Authorization") ||
           sliceEqualsIgnoreCase(name, "Cookie") ||
           sliceEqualsIgnoreCase(name, "Set-Cookie") ||
           sliceEqualsIgnoreCase(name, "X-API-Key") ||
           sliceEqualsIgnoreCase(name, "X-Auth-Token") ||
           sliceEqualsIgnoreCase(name, "WWW-Authenticate") ||
           sliceEqualsIgnoreCase(name, "Proxy-Authenticate");
}

static void httpParserTraceHeaders(const HttpParser* parser) {
    if (parser == NULL || !parser->headerComplete)
        return;

    char reason[81];
    httpParserSafeText(reason, sizeof(reason),
                       parser->response.reasonPhrase.data,
                       parser->response.reasonPhrase.len);
    LOG_TRACE("HTTP PARSER RX: HTTP/1.%d status=%d reason=%s headers=%lu",
              parser->response.version == HTTP_VERSION_1_0 ? 0 : 1,
              parser->response.statusCode, reason,
              (unsigned long)parser->response.headers.count);

    for (size_t i = 0u; i < parser->response.headers.count; i++) {
        const HttpHeader* h = &parser->response.headers.headers[i];
        char              name[81];
        char              value[161];
        httpParserSafeText(name, sizeof(name), h->name.data, h->name.len);
        if (httpParserSensitiveHeader(h->name)) {
            LOG_TRACE("HTTP PARSER RX HEADER[%lu]: %s: [REDACTED]",
                      (unsigned long)i, name);
        } else {
            httpParserSafeText(value, sizeof(value), h->value.data,
                               h->value.len);
            LOG_TRACE("HTTP PARSER RX HEADER[%lu]: %s: %s%s", (unsigned long)i,
                      name, value,
                      h->value.len >= sizeof(value) ? " [TRUNCATED]" : "");
        }
    }
    LOG_TRACE("HTTP PARSER RX FRAME: chunked=%d hasContentLength=%d "
              "contentLength=%lu connectionClose=%d bodyForbidden=%d",
              (int)parser->chunked, (int)parser->hasContentLength,
              (unsigned long)parser->contentLength,
              (int)parser->connectionClose,
              (int)parser->response.bodyForbidden);
}
#endif

#if HTTP_PARSER_TRACE_BODY
static void httpParserTraceBody(const HttpParser* parser, const uint8_t* data,
                                size_t totalLength) {
    if (parser == NULL || data == NULL || !parser->messageComplete ||
        !parser->headerComplete || parser->response.bodyForbidden)
        return;
    if (parser->headerBytes > totalLength) {
        LOG_ERROR("HTTP PARSER RX BODY: bad offset headerBytes=%lu rxLen=%lu",
                  (unsigned long)parser->headerBytes,
                  (unsigned long)totalLength);
        return;
    }

    /* Chunk framing has already been decoded in place by Pico. */
    size_t available = totalLength - parser->headerBytes;
    size_t length    = parser->bodyReceived;
    if (length > available) {
        LOG_ERROR(
            "HTTP PARSER RX BODY: invalid decoded length=%lu available=%lu",
            (unsigned long)length, (unsigned long)available);
        return;
    }
    const uint8_t* body  = data + parser->headerBytes;
    size_t         shown = length;
    if (shown > HTTP_PARSER_TRACE_MAX_BYTES)
        shown = HTTP_PARSER_TRACE_MAX_BYTES;

    LOG_TRACE("HTTP PARSER RX BODY: decodedLen=%lu showing=%lu chunked=%d",
              (unsigned long)length, (unsigned long)shown,
              (int)parser->chunked);
    if (shown == 0u)
        return;

    bool textBody = true;
    for (size_t i = 0u; i < shown; i++) {
        uint8_t c = body[i];
        if (!(c >= 32u && c <= 126u) && c != '\r' && c != '\n' && c != '\t') {
            textBody = false;
            break;
        }
    }

    /* Limit each LOG_TRACE argument to a small on-stack buffer. */
    for (size_t offset = 0u; offset < shown;) {
        size_t segment = shown - offset;
        if (textBody) {
            char line[81];
            if (segment > sizeof(line) - 1u)
                segment = sizeof(line) - 1u;
            for (size_t j = 0u; j < segment; j++) {
                uint8_t c = body[offset + j];
                line[j] = (c == '\r' || c == '\n' || c == '\t') ? ' ' : (char)c;
            }
            line[segment] = '\0';
            LOG_TRACE("HTTP PARSER RX BODY TEXT[%lu]: %s",
                      (unsigned long)offset, line);
        } else {
            char              hex[3u * 16u + 1u];
            static const char digits[] = "0123456789ABCDEF";
            if (segment > 16u)
                segment = 16u;
            for (size_t j = 0u; j < segment; j++) {
                uint8_t c        = body[offset + j];
                hex[3u * j]      = digits[c >> 4u];
                hex[3u * j + 1u] = digits[c & 0x0Fu];
                hex[3u * j + 2u] = ' ';
            }
            hex[segment * 3u] = '\0';
            LOG_TRACE("HTTP PARSER RX BODY HEX[%lu]: %s", (unsigned long)offset,
                      hex);
        }
        offset += segment;
    }
    if (shown < length)
        LOG_TRACE("HTTP PARSER RX BODY: truncated %lu byte(s)",
                  (unsigned long)(length - shown));
}
#endif

static bool parseContentLength(HttpSlice value, size_t* result) {
    size_t valueNumber = 0;

    if (value.data == NULL || result == NULL || value.len == 0) {
        LOG_ERROR("HTTP PARSER parseContentLength(): invalid input len=%lu",
                  (unsigned long)value.len);
        return false;
    }

    for (size_t i = 0; i < value.len; i++) {

        char c = value.data[i];

        if (c < '0' || c > '9') {
            LOG_ERROR("HTTP PARSER parseContentLength(): invalid digit at %lu",
                      (unsigned long)i);
            return false;
        }

        size_t digit = (size_t)(c - '0');

        if (valueNumber > (SIZE_MAX - digit) / 10U) {
            LOG_ERROR("HTTP PARSER parseContentLength(): integer overflow");
            return false;
        }

        valueNumber = valueNumber * 10U + digit;
    }

    *result = valueNumber;

    return true;
}

static HttpSlice trimOptionalWhitespace(HttpSlice slice) {
    while (slice.len > 0U && (*slice.data == ' ' || *slice.data == '\t')) {
        slice.data++;
        slice.len--;
    }

    while (slice.len > 0U && (slice.data[slice.len - 1U] == ' ' ||
                              slice.data[slice.len - 1U] == '\t')) {
        slice.len--;
    }

    return slice;
}

static int hexDigitValue(uint8_t value) {
    if (value >= '0' && value <= '9')
        return (int)(value - '0');

    if (value >= 'a' && value <= 'f')
        return (int)(value - 'a') + 10;

    if (value >= 'A' && value <= 'F')
        return (int)(value - 'A') + 10;

    return -1;
}

static HttpParseResult findCrlf(const uint8_t* data, size_t start, size_t len,
                                size_t maximumLineLength, size_t* lineEnd) {
    if (data == NULL || lineEnd == NULL || start > len) {
        LOG_ERROR(
            "HTTP PARSER findCrlf(): invalid pointers or start=%lu len=%lu",
            (unsigned long)start, (unsigned long)len);
        return HTTP_PARSE_ERROR;
    }

    for (size_t i = start; i < len; i++) {

        if (i - start > maximumLineLength) {
            LOG_ERROR("HTTP PARSER findCrlf(): line too long limit=%lu",
                      (unsigned long)maximumLineLength);
            return HTTP_PARSE_OVERFLOW;
        }

        if (data[i] == '\n') {
            LOG_ERROR("HTTP PARSER findCrlf(): bare LF at offset=%lu",
                      (unsigned long)i);
            return HTTP_PARSE_ERROR;
        }

        if (data[i] != '\r')
            continue;

        if (i + 1U >= len)
            return HTTP_PARSE_INCOMPLETE;

        if (data[i + 1U] != '\n') {
            LOG_ERROR("HTTP PARSER findCrlf(): CR without LF offset=%lu",
                      (unsigned long)i);
            return HTTP_PARSE_ERROR;
        }

        *lineEnd = i;
        return HTTP_PARSE_OK;
    }

    if (len - start > maximumLineLength) {
        LOG_ERROR("HTTP PARSER findCrlf(): incomplete line exceeds limit=%lu",
                  (unsigned long)maximumLineLength);
        return HTTP_PARSE_OVERFLOW;
    }

    return HTTP_PARSE_INCOMPLETE;
}

static HttpParseResult parseChunkSize(const uint8_t* data, size_t start,
                                      size_t lineEnd, size_t* chunkSize) {
    size_t value    = 0U;
    size_t i        = start;
    bool   hasDigit = false;

    if (data == NULL || chunkSize == NULL || start > lineEnd) {
        LOG_ERROR("HTTP PARSER parseChunkSize(): invalid arguments");
        return HTTP_PARSE_ERROR;
    }

    while (i < lineEnd) {
        int digit = hexDigitValue(data[i]);

        if (digit < 0)
            break;

        hasDigit = true;

        if (value > (SIZE_MAX - (size_t)digit) / 16U) {
            LOG_ERROR("HTTP PARSER parseChunkSize(): hex size overflow");
            return HTTP_PARSE_OVERFLOW;
        }

        value = value * 16U + (size_t)digit;
        i++;
    }

    if (!hasDigit) {
        LOG_ERROR("HTTP PARSER parseChunkSize(): missing hex size");
        return HTTP_PARSE_ERROR;
    }

    /*
     * Anything after the hexadecimal size must be a chunk extension.
     * PicoHTTPParser will fully validate it during final decoding.
     */
    if (i < lineEnd && data[i] != ';') {
        LOG_ERROR(
            "HTTP PARSER parseChunkSize(): malformed extension offset=%lu",
            (unsigned long)i);
        return HTTP_PARSE_ERROR;
    }

    *chunkSize = value;
    return HTTP_PARSE_OK;
}

static HttpParseResult scanChunkedBody(const uint8_t* data, size_t len,
                                       size_t bodyOffset, size_t* decodedLength,
                                       size_t* encodedLength) {
    size_t position   = bodyOffset;
    size_t decoded    = 0U;
    size_t chunkCount = 0U;

    if (data == NULL || decodedLength == NULL || encodedLength == NULL ||
        bodyOffset > len) {
        LOG_ERROR("HTTP PARSER scanChunkedBody(): invalid input len=%lu "
                  "bodyOffset=%lu",
                  (unsigned long)len, (unsigned long)bodyOffset);
        return HTTP_PARSE_ERROR;
    }

    *decodedLength = 0U;
    *encodedLength = 0U;

    for (;;) {
        size_t lineEnd;
        size_t chunkSize;

        HttpParseResult result =
            findCrlf(data, position, len, HTTP_MAX_CHUNK_LINE_BYTES, &lineEnd);

        *decodedLength = decoded;

        if (result != HTTP_PARSE_OK)
            return result;

        result = parseChunkSize(data, position, lineEnd, &chunkSize);

        if (result != HTTP_PARSE_OK)
            return result;

        position = lineEnd + 2U;

        /*
         * Final zero-sized chunk. It is followed by zero or more
         * trailer fields and then one empty CRLF line.
         */
        if (chunkSize == 0U) {
            size_t trailerStart = position;

            for (;;) {
                size_t trailerLineStart = position;

                if (position - trailerStart > HTTP_MAX_TRAILER_BYTES) {
                    LOG_ERROR(
                        "HTTP PARSER scanChunkedBody(): trailer overflow");
                    return HTTP_PARSE_OVERFLOW;
                }

                result = findCrlf(data, position, len, HTTP_MAX_TRAILER_BYTES,
                                  &lineEnd);

                if (result != HTTP_PARSE_OK) {
                    if (result == HTTP_PARSE_INCOMPLETE &&
                        len - trailerStart > HTTP_MAX_TRAILER_BYTES) {
                        LOG_ERROR("HTTP PARSER scanChunkedBody(): incomplete "
                                  "trailer exceeds limit");
                        return HTTP_PARSE_OVERFLOW;
                    }

                    return result;
                }

                position = lineEnd + 2U;

                if (position - trailerStart > HTTP_MAX_TRAILER_BYTES) {
                    LOG_ERROR(
                        "HTTP PARSER scanChunkedBody(): trailer overflow");
                    return HTTP_PARSE_OVERFLOW;
                }

                if (lineEnd == trailerLineStart) {
                    *decodedLength = decoded;
                    *encodedLength = position - bodyOffset;
                    LOG_TRACE("HTTP PARSER scanChunkedBody(): complete "
                              "chunks=%lu decoded=%lu encoded=%lu",
                              (unsigned long)chunkCount, (unsigned long)decoded,
                              (unsigned long)*encodedLength);
                    return HTTP_PARSE_OK;
                }
            }
        }

        chunkCount++;

        if (chunkCount > HTTP_MAX_CHUNKS) {
            LOG_ERROR("HTTP PARSER scanChunkedBody(): too many chunks=%lu",
                      (unsigned long)chunkCount);
            return HTTP_PARSE_OVERFLOW;
        }

        if (decoded > HTTP_MAX_BODY_SIZE ||
            chunkSize > HTTP_MAX_BODY_SIZE - decoded) {
            LOG_ERROR("HTTP PARSER scanChunkedBody(): body exceeds limit "
                      "decoded=%lu chunk=%lu",
                      (unsigned long)decoded, (unsigned long)chunkSize);
            return HTTP_PARSE_OVERFLOW;
        }

        if (chunkSize > len - position)
            return HTTP_PARSE_INCOMPLETE;

        position += chunkSize;

        /*
         * Every non-final chunk payload must end with CRLF.
         */
        if (len - position < 2U)
            return HTTP_PARSE_INCOMPLETE;

        if (data[position] != '\r' || data[position + 1U] != '\n') {
            LOG_ERROR(
                "HTTP PARSER scanChunkedBody(): bad chunk CRLF offset=%lu",
                (unsigned long)position);
            return HTTP_PARSE_ERROR;
        }

        decoded += chunkSize;
        position += 2U;
    }
}

static HttpParseResult parseChunkedBody(HttpParser* parser, uint8_t* data,
                                        size_t len) {
    struct phr_chunked_decoder decoder;
    size_t                     decodedLength;
    size_t                     encodedLength;
    size_t                     picoLength;
    intptr_t                   picoResult;

    HttpParseResult result = scanChunkedBody(data, len, parser->headerBytes,
                                             &decodedLength, &encodedLength);

    parser->bodyReceived    = decodedLength;
    parser->messageComplete = false;

    if (result != HTTP_PARSE_OK) {
        if (result != HTTP_PARSE_INCOMPLETE)
            LOG_ERROR(
                "HTTP PARSER parseChunkedBody(): scan failed result=%s(%d)",
                httpParseResultText(result), (int)result);
        return result;
    }

    LOG_TRACE("HTTP PARSER parseChunkedBody(): decoding encoded=%lu "
              "expectedDecoded=%lu",
              (unsigned long)encodedLength, (unsigned long)decodedLength);
    memset(&decoder, 0, sizeof(decoder));

    /*
     * Discard trailers after validating their chunk framing.
     */
    decoder.consume_trailer = 1;

    picoLength = encodedLength;

    picoResult = phr_decode_chunked(&decoder, (char*)data + parser->headerBytes,
                                    &picoLength);

    /*
     * We supplied exactly one complete encoded message, therefore
     * Pico must report zero trailing bytes.
     */
    if (picoResult != 0 || picoLength != decodedLength ||
        picoLength > HTTP_MAX_BODY_SIZE) {
        LOG_ERROR("HTTP PARSER parseChunkedBody(): decoder mismatch "
                  "picoResult=%ld decoded=%lu expected=%lu",
                  (long)picoResult, (unsigned long)picoLength,
                  (unsigned long)decodedLength);
        parser->messageComplete = false;
        return HTTP_PARSE_ERROR;
    }

    parser->bodyReceived    = picoLength;
    parser->messageComplete = true;
    LOG_TRACE("HTTP PARSER parseChunkedBody(): decoded successfully bytes=%lu",
              (unsigned long)picoLength);

    return HTTP_PARSE_OK;
}

static void resetResponse(HttpResponse* response) {
    memset(response, 0, sizeof(*response));

    response->version = HTTP_VERSION_1_1;
}

void httpParserInit(HttpParser* parser) {
    if (parser == NULL) {
        LOG_ERROR("HTTP PARSER httpParserInit(): parser is NULL");
        return;
    }

    LOG_TRACE("HTTP PARSER httpParserInit(): reset parser");
    memset(parser, 0, sizeof(*parser));

    resetResponse(&parser->response);
}

static HttpParseResult parseHeaders(HttpParser*              parser,
                                    const struct phr_header* headers,
                                    size_t                   headerCount) {
    if (parser == NULL || headers == NULL) {
        LOG_ERROR("HTTP PARSER parseHeaders(): invalid arguments");
        return HTTP_PARSE_ERROR;
    }

    if (headerCount > HTTP_MAX_HEADERS) {
        LOG_ERROR(
            "HTTP PARSER parseHeaders(): too many headers count=%lu max=%lu",
            (unsigned long)headerCount, (unsigned long)HTTP_MAX_HEADERS);
        return HTTP_PARSE_OVERFLOW;
    }

    LOG_TRACE("HTTP PARSER parseHeaders(): headerCount=%lu",
              (unsigned long)headerCount);

    parser->response.headers.count = 0;

    for (size_t i = 0; i < headerCount; i++) {

        const struct phr_header* src = &headers[i];

        if (src->name == NULL)
            continue;

        if (src->name_len > HTTP_MAX_HEADER_NAME_LEN ||
            src->value_len > HTTP_MAX_HEADER_VALUE_LEN) {
            LOG_ERROR("HTTP PARSER parseHeaders(): header[%lu] nameLen=%lu "
                      "valueLen=%lu exceeds maximum",
                      (unsigned long)i, (unsigned long)src->name_len,
                      (unsigned long)src->value_len);
            return HTTP_PARSE_OVERFLOW;
        }

        HttpHeader* dst =
            &parser->response.headers.headers[parser->response.headers.count];

        dst->name.data = src->name;
        dst->name.len  = src->name_len;

        dst->value.data = src->value;
        dst->value.len  = src->value_len;

        parser->response.headers.count++;

        /*
         * Content-Length
         */
        if (sliceEqualsIgnoreCase(dst->name, "Content-Length")) {

            size_t contentLength;

            if (!parseContentLength(dst->value, &contentLength)) {
                LOG_ERROR("HTTP PARSER parseHeaders(): invalid Content-Length");
                return HTTP_PARSE_ERROR;
            }

            if (contentLength > HTTP_MAX_BODY_SIZE) {
                LOG_ERROR("HTTP PARSER parseHeaders(): Content-Length=%lu "
                          "exceeds max=%lu",
                          (unsigned long)contentLength,
                          (unsigned long)HTTP_MAX_BODY_SIZE);
                return HTTP_PARSE_OVERFLOW;
            }

            if (parser->hasContentLength) {
                if (parser->contentLength != contentLength) {
                    LOG_ERROR("HTTP PARSER parseHeaders(): conflicting "
                              "Content-Length %lu vs %lu",
                              (unsigned long)parser->contentLength,
                              (unsigned long)contentLength);
                    return HTTP_PARSE_ERROR;
                }
            }

            parser->hasContentLength       = true;
            parser->contentLength          = contentLength;
            parser->response.contentLength = contentLength;
            LOG_TRACE("HTTP PARSER parseHeaders(): Content-Length=%lu",
                      (unsigned long)contentLength);
        }

        /*
         * Transfer-Encoding
         */
        else if (sliceEqualsIgnoreCase(dst->name, "Transfer-Encoding")) {
            HttpSlice encoding = trimOptionalWhitespace(dst->value);

            /*
             * This implementation supports only plain chunked encoding.
             * For example, "gzip, chunked" remains unsupported.
             */
            if (!sliceEqualsIgnoreCase(encoding, "chunked")) {
                LOG_ERROR("HTTP PARSER parseHeaders(): unsupported "
                          "Transfer-Encoding");
                return HTTP_PARSE_UNSUPPORTED;
            }

            /*
             * Chunked must not be applied more than once.
             */
            if (parser->chunked) {
                LOG_ERROR("HTTP PARSER parseHeaders(): duplicate chunked "
                          "Transfer-Encoding");
                return HTTP_PARSE_ERROR;
            }

            LOG_TRACE("HTTP PARSER parseHeaders(): Transfer-Encoding=chunked");

            parser->chunked          = true;
            parser->response.chunked = true;
        }

        /*
         * Connection
         */
        else if (sliceEqualsIgnoreCase(dst->name, "Connection")) {

            if (sliceEqualsIgnoreCase(dst->value, "close")) {

                parser->connectionClose          = true;
                parser->response.connectionClose = true;
                LOG_TRACE("HTTP PARSER parseHeaders(): Connection=close");
            }

            else if (sliceEqualsIgnoreCase(dst->value, "keep-alive")) {

                parser->response.connectionKeepAlive = true;
                LOG_TRACE("HTTP PARSER parseHeaders(): Connection=keep-alive");
            }
        }
    }

    /*
     * Reject ambiguous response framing.
     */
    if (parser->chunked && parser->hasContentLength) {
        LOG_ERROR("HTTP PARSER parseHeaders(): ambiguous framing: chunked + "
                  "Content-Length");
        return HTTP_PARSE_ERROR;
    }

    return HTTP_PARSE_OK;
}

static HttpParseResult httpParserParseResponseImpl(HttpParser* parser,
                                                   uint8_t* data, size_t len) {
    RETURN_VALUE_IF_NULL(parser, ;, HTTP_PARSE_ERROR);
    RETURN_VALUE_IF_NULL(data, ;, HTTP_PARSE_ERROR);

    if (parser->messageComplete)
        return HTTP_PARSE_OK;

    RETURN_VALUE_IF(len, 0, ;, HTTP_PARSE_INCOMPLETE);

    /*
     * Once headers have been parsed, do not call phr_parse_response() again.
     *
     * This API receives the same accumulated buffer with an increasing length.
     * Header metadata is already available, so only body progress needs
     * updating.
     */
    if (parser->headerComplete) {
        size_t availableBody;

        /*
         * The accumulated buffer must never become shorter.
         */
        if (len < parser->parsedBytesPreviously || parser->headerBytes > len) {
            LOG_ERROR("HTTP PARSER parseResponse(): RX buffer shrank "
                      "previous=%lu current=%lu headerBytes=%lu",
                      (unsigned long)parser->parsedBytesPreviously,
                      (unsigned long)len, (unsigned long)parser->headerBytes);
            return HTTP_PARSE_ERROR;
        }

        parser->parsedBytesPreviously = len;

        if (parser->response.bodyForbidden) {
            LOG_TRACE("HTTP PARSER parseResponse(): body forbidden; complete");
            parser->bodyReceived    = 0u;
            parser->messageComplete = true;
            return HTTP_PARSE_OK;
        }

        if (parser->chunked)
            return parseChunkedBody(parser, data, len);

        availableBody = len - parser->headerBytes;

        if (availableBody > HTTP_MAX_BODY_SIZE) {
            LOG_ERROR("HTTP PARSER parseResponse(): body exceeds max=%lu "
                      "available=%lu",
                      (unsigned long)HTTP_MAX_BODY_SIZE,
                      (unsigned long)availableBody);
            return HTTP_PARSE_OVERFLOW;
        }

        if (parser->hasContentLength) {
            if (availableBody >= parser->contentLength) {
                LOG_TRACE("HTTP PARSER parseResponse(): Content-Length "
                          "complete received=%lu expected=%lu",
                          (unsigned long)availableBody,
                          (unsigned long)parser->contentLength);
                parser->bodyReceived    = parser->contentLength;
                parser->messageComplete = true;
                return HTTP_PARSE_OK;
            }

            parser->bodyReceived    = availableBody;
            parser->messageComplete = false;

            return HTTP_PARSE_INCOMPLETE;
        }

        /*
         * A connection-close-delimited body cannot be marked complete until
         * the transport explicitly reports a clean EOF.
         */
        parser->bodyReceived    = availableBody;
        parser->messageComplete = false;
        return HTTP_PARSE_INCOMPLETE;
    }

    /*
     * PicoHTTPParser writes pointers into the supplied buffer.
     * No HTTP data is copied here.
     */

    int minorVersion = 0;
    int status       = 0;

    const char* msg    = NULL;
    size_t      msgLen = 0;

    /*
     * One extra slot lets us distinguish “too many headers” from malformed
     * HTTP.
     */
    struct phr_header headers[HTTP_MAX_HEADERS + 1U];
    size_t            headerCount = HTTP_MAX_HEADERS + 1U;

    int ret = phr_parse_response((const char*)data, len, &minorVersion, &status,
                                 &msg, &msgLen, headers, &headerCount,
                                 parser->parsedBytesPreviously);
    if (ret != -2)
        LOG_TRACE(
            "HTTP PARSER parseResponse(): pico result=%d rxLen=%lu headers=%lu",
            ret, (unsigned long)len, (unsigned long)headerCount);

    /*
     * Save the amount of data supplied to this attempt.
     *
     * This is important for incremental parsing.
     */
    parser->parsedBytesPreviously = len;

    /*
     * Once the buffer reaches the configured header limit without
     * producing a valid complete header, classify it as overflow.
     *
     * Check this before distinguishing Pico's -2 and -1 results.
     */

    if (ret < 0 && len >= HTTP_MAX_HEADER_BYTES) {
        LOG_ERROR("HTTP PARSER parseResponse(): header limit exceeded "
                  "rxLen=%lu max=%lu pico=%d",
                  (unsigned long)len, (unsigned long)HTTP_MAX_HEADER_BYTES,
                  ret);
        return HTTP_PARSE_OVERFLOW;
    }

    if (ret == -2)
        return HTTP_PARSE_INCOMPLETE;
    /*
     * Invalid HTTP response.
     */
    if (ret < 0) {
        LOG_ERROR(
            "HTTP PARSER parseResponse(): malformed status/headers pico=%d",
            ret);
        return HTTP_PARSE_ERROR;
    }

    if ((size_t)ret > HTTP_MAX_HEADER_BYTES || msgLen > HTTP_MAX_TARGET_LEN) {
        LOG_ERROR("HTTP PARSER parseResponse(): oversized status/header "
                  "bytes=%d reasonLen=%lu",
                  ret, (unsigned long)msgLen);
        return HTTP_PARSE_OVERFLOW;
    }

    /*
     * Header has been parsed successfully.
     */
    parser->headerComplete = true;
    parser->headerBytes    = (size_t)ret;

    parser->response.headerBytes = (size_t)ret;

    parser->response.statusCode = status;

    parser->response.reasonPhrase.data = msg;
    parser->response.reasonPhrase.len  = msgLen;

    parser->response.version =
        (minorVersion == 0) ? HTTP_VERSION_1_0 : HTTP_VERSION_1_1;

    LOG_TRACE("HTTP PARSER parseResponse(): HTTP/1.%d status=%d headerBytes=%d",
              minorVersion, status, ret);

    HttpParseResult result = parseHeaders(parser, headers, headerCount);

    if (result != HTTP_PARSE_OK) {
        LOG_ERROR("HTTP PARSER parseResponse(): headers rejected result=%s(%d)",
                  httpParseResultText(result), (int)result);
        return result;
    }

    /*
     * HTTP responses without a body.
     *
     * 1xx, 204 and 304 do not contain a message body.
     */
    if ((status >= 100 && status < 200) || status == 204 || status == 304) {

        parser->response.bodyForbidden = true;
        parser->messageComplete        = true;
        LOG_TRACE("HTTP PARSER parseResponse(): status=%d forbids body",
                  status);

        return HTTP_PARSE_OK;
    }

    /*
     * Chunked responses are not considered complete here.
     *
     * Chunk decoding should be handled by a dedicated body decoder.
     */

    if (parser->chunked)
        return parseChunkedBody(parser, data, len);

    /*
     * Content-Length response.
     */
    if (parser->hasContentLength) {

        size_t availableBody;

        if (ret < 0 || (size_t)ret > len) {
            LOG_ERROR("HTTP PARSER parseResponse(): headerBytes=%d exceeds RX "
                      "len=%lu",
                      ret, (unsigned long)len);
            return HTTP_PARSE_ERROR;
        }

        availableBody = len - (size_t)ret;

        parser->bodyReceived = availableBody;
        LOG_TRACE(
            "HTTP PARSER parseResponse(): first body bytes=%lu expected=%lu",
            (unsigned long)availableBody, (unsigned long)parser->contentLength);

        if (availableBody >= parser->contentLength) {
            parser->bodyReceived    = parser->contentLength;
            parser->messageComplete = true;
            return HTTP_PARSE_OK;
        }

        return HTTP_PARSE_INCOMPLETE;
    }

    /*
     * Track bytes for a response whose body will be delimited by EOF.
     */
    {
        size_t availableBody = len - (size_t)ret;

        RETURN_VALUE_IF_GREATER(availableBody, HTTP_MAX_BODY_SIZE, ;
                                , HTTP_PARSE_OVERFLOW);

        parser->bodyReceived = availableBody;
    }

    /*
     * No Content-Length and not chunked.
     *
     * For HTTP/1.0/Connection: close responses the body is
     * delimited by connection close.
     *
     * We therefore cannot mark it complete until nth detects
     * socket closure.
     */

    RETURN_VALUE_IF((parser->connectionClose || minorVersion == 0), true,
                    parser->messageComplete = false;
                    , HTTP_PARSE_INCOMPLETE);

    /*
     * HTTP/1.1 response without a body delimiter is not safe
     * to assume complete.
     */
    parser->messageComplete = false;
    LOG_TRACE(
        "HTTP PARSER parseResponse(): waiting for EOF (no length delimiter)");

    return HTTP_PARSE_INCOMPLETE;
}

/*
 * Trace transitions once. The NTH layer calls with an accumulated buffer:
 * printing it on each invocation would duplicate earlier bytes.
 */
HttpParseResult httpParserParseResponse(HttpParser* parser, uint8_t* data,
                                        size_t len) {
#if HTTP_PARSER_TRACE_HEADERS
    bool hadHeaders = parser != NULL && parser->headerComplete;
#endif
    bool   wasComplete  = parser != NULL && parser->messageComplete;
    size_t previousBody = parser != NULL ? parser->bodyReceived : 0u;

    HttpParseResult result = httpParserParseResponseImpl(parser, data, len);

    if (parser != NULL) {
#if HTTP_PARSER_TRACE_HEADERS
        if (!hadHeaders && parser->headerComplete &&
            (result == HTTP_PARSE_OK || result == HTTP_PARSE_INCOMPLETE))
            httpParserTraceHeaders(parser);
#endif
        if (result != HTTP_PARSE_OK && result != HTTP_PARSE_INCOMPLETE) {
            LOG_ERROR("HTTP PARSER parseResponse(): FAILED result=%s(%d) "
                      "rxLen=%lu headerComplete=%d bodyReceived=%lu",
                      httpParseResultText(result), (int)result,
                      (unsigned long)len, (int)parser->headerComplete,
                      (unsigned long)parser->bodyReceived);
        } else if (!wasComplete && parser->messageComplete) {
            LOG_TRACE("HTTP PARSER parseResponse(): COMPLETE status=%d "
                      "body=%lu totalRx=%lu",
                      parser->response.statusCode,
                      (unsigned long)parser->bodyReceived, (unsigned long)len);
#if HTTP_PARSER_TRACE_BODY
            httpParserTraceBody(parser, data, len);
#endif
        } else if (parser->bodyReceived != previousBody) {
            LOG_TRACE("HTTP PARSER parseResponse(): body progress=%lu bytes",
                      (unsigned long)parser->bodyReceived);
        }
    }
    return result;
}

HttpParseResult httpParserFinish(HttpParser* parser) {
    if (parser == NULL) {
        LOG_ERROR("HTTP PARSER httpParserFinish(): parser is NULL");
        return HTTP_PARSE_ERROR;
    }

    LOG_TRACE("HTTP PARSER httpParserFinish(): EOF headers=%d complete=%d "
              "chunked=%d contentLength=%lu received=%lu",
              (int)parser->headerComplete, (int)parser->messageComplete,
              (int)parser->chunked, (unsigned long)parser->contentLength,
              (unsigned long)parser->bodyReceived);

    if (!parser->headerComplete) {
        LOG_ERROR(
            "HTTP PARSER httpParserFinish(): EOF before complete headers");
        return HTTP_PARSE_ERROR;
    }

    if (parser->messageComplete)
        return HTTP_PARSE_OK;

    /*
     * EOF is not a valid replacement for the terminal zero chunk.
     */
    if (parser->chunked) {
        LOG_ERROR(
            "HTTP PARSER httpParserFinish(): EOF before terminal zero chunk");
        return HTTP_PARSE_ERROR;
    }

    /*
     * EOF before Content-Length is a truncated response.
     */
    if (parser->hasContentLength) {
        if (parser->bodyReceived != parser->contentLength) {
            LOG_ERROR("HTTP PARSER httpParserFinish(): truncated body "
                      "expected=%lu received=%lu",
                      (unsigned long)parser->contentLength,
                      (unsigned long)parser->bodyReceived);
            return HTTP_PARSE_ERROR;
        }

        parser->messageComplete = true;
        LOG_TRACE(
            "HTTP PARSER httpParserFinish(): complete Content-Length body");
        return HTTP_PARSE_OK;
    }

    if (parser->bodyReceived > HTTP_MAX_BODY_SIZE) {
        LOG_ERROR("HTTP PARSER httpParserFinish(): body overflow received=%lu "
                  "max=%lu",
                  (unsigned long)parser->bodyReceived,
                  (unsigned long)HTTP_MAX_BODY_SIZE);
        return HTTP_PARSE_OVERFLOW;
    }

    parser->messageComplete = true;
    LOG_TRACE("HTTP PARSER httpParserFinish(): close-delimited body completed "
              "bytes=%lu",
              (unsigned long)parser->bodyReceived);
    return HTTP_PARSE_OK;
}

bool httpParserIsHeaderComplete(const HttpParser* parser) {
    if (parser == NULL)
        return false;

    return parser->headerComplete;
}

bool httpParserIsMessageComplete(const HttpParser* parser) {
    if (parser == NULL)
        return false;

    return parser->messageComplete;
}

const HttpResponse* httpParserGetResponse(const HttpParser* parser) {
    if (parser == NULL)
        return NULL;

    return &parser->response;
}

const uint8_t* httpParserGetBody(const HttpParser* parser, const uint8_t* data,
                                 size_t len) {
    if (parser == NULL || data == NULL)
        return NULL;

    if (!parser->headerComplete)
        return NULL;

    if (parser->response.bodyForbidden)
        return NULL;

    if (parser->headerBytes > len)
        return NULL;

    return data + parser->headerBytes;
}

size_t httpParserGetBodyLength(const HttpParser* parser, size_t totalLen) {
    RETURN_VALUE_IF_NULL(parser, ;, 0);
    RETURN_VALUE_IF_NOT(parser->headerComplete, true, ;, 0);

    RETURN_VALUE_IF(parser->response.bodyForbidden, true, ;, 0);

    RETURN_VALUE_IF_GREATER(parser->headerBytes, totalLen, ;, 0);

    /*
     * Chunk framing has been removed in place. totalLen still describes
     * the original encoded RX length, so it cannot be used here.
     */
    if (parser->chunked)
        return parser->messageComplete ? parser->bodyReceived : 0U;

    size_t available = totalLen - parser->headerBytes;

    if (parser->hasContentLength && parser->contentLength < available) {
        return parser->contentLength;
    }

    return available;
}