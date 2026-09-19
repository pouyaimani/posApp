#include "httpParser.h"
#include "logger.h"
#include "error.h"

#include <string.h>

#include "picohttpparser/picohttpparser.h"

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

static bool parseContentLength(HttpSlice value, size_t* result) {
    size_t valueNumber = 0;

    if (value.data == NULL || result == NULL || value.len == 0)
        return false;

    for (size_t i = 0; i < value.len; i++) {

        char c = value.data[i];

        if (c < '0' || c > '9')
            return false;

        size_t digit = (size_t)(c - '0');

        if (valueNumber > (SIZE_MAX - digit) / 10U) {
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
    if (data == NULL || lineEnd == NULL || start > len)
        return HTTP_PARSE_ERROR;

    for (size_t i = start; i < len; i++) {

        if (i - start > maximumLineLength)
            return HTTP_PARSE_OVERFLOW;

        if (data[i] == '\n')
            return HTTP_PARSE_ERROR;

        if (data[i] != '\r')
            continue;

        if (i + 1U >= len)
            return HTTP_PARSE_INCOMPLETE;

        if (data[i + 1U] != '\n')
            return HTTP_PARSE_ERROR;

        *lineEnd = i;
        return HTTP_PARSE_OK;
    }

    if (len - start > maximumLineLength)
        return HTTP_PARSE_OVERFLOW;

    return HTTP_PARSE_INCOMPLETE;
}

static HttpParseResult parseChunkSize(const uint8_t* data, size_t start,
                                      size_t lineEnd, size_t* chunkSize) {
    size_t value    = 0U;
    size_t i        = start;
    bool   hasDigit = false;

    if (data == NULL || chunkSize == NULL || start > lineEnd)
        return HTTP_PARSE_ERROR;

    while (i < lineEnd) {
        int digit = hexDigitValue(data[i]);

        if (digit < 0)
            break;

        hasDigit = true;

        if (value > (SIZE_MAX - (size_t)digit) / 16U)
            return HTTP_PARSE_OVERFLOW;

        value = value * 16U + (size_t)digit;
        i++;
    }

    if (!hasDigit)
        return HTTP_PARSE_ERROR;

    /*
     * Anything after the hexadecimal size must be a chunk extension.
     * PicoHTTPParser will fully validate it during final decoding.
     */
    if (i < lineEnd && data[i] != ';')
        return HTTP_PARSE_ERROR;

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
                    return HTTP_PARSE_OVERFLOW;
                }

                result = findCrlf(data, position, len, HTTP_MAX_TRAILER_BYTES,
                                  &lineEnd);

                if (result != HTTP_PARSE_OK) {
                    if (result == HTTP_PARSE_INCOMPLETE &&
                        len - trailerStart > HTTP_MAX_TRAILER_BYTES) {
                        return HTTP_PARSE_OVERFLOW;
                    }

                    return result;
                }

                position = lineEnd + 2U;

                if (position - trailerStart > HTTP_MAX_TRAILER_BYTES) {
                    return HTTP_PARSE_OVERFLOW;
                }

                if (lineEnd == trailerLineStart) {
                    *decodedLength = decoded;
                    *encodedLength = position - bodyOffset;
                    return HTTP_PARSE_OK;
                }
            }
        }

        chunkCount++;

        if (chunkCount > HTTP_MAX_CHUNKS)
            return HTTP_PARSE_OVERFLOW;

        if (chunkSize > HTTP_MAX_BODY_SIZE - decoded)
            return HTTP_PARSE_OVERFLOW;

        if (chunkSize > len - position)
            return HTTP_PARSE_INCOMPLETE;

        position += chunkSize;

        /*
         * Every non-final chunk payload must end with CRLF.
         */
        if (len - position < 2U)
            return HTTP_PARSE_INCOMPLETE;

        if (data[position] != '\r' || data[position + 1U] != '\n') {
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

    if (result != HTTP_PARSE_OK)
        return result;

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
        parser->messageComplete = false;
        return HTTP_PARSE_ERROR;
    }

    parser->bodyReceived    = picoLength;
    parser->messageComplete = true;

    return HTTP_PARSE_OK;
}

static void resetResponse(HttpResponse* response) {
    memset(response, 0, sizeof(*response));

    response->version = HTTP_VERSION_1_1;
}

void httpParserInit(HttpParser* parser) {
    if (parser == NULL)
        return;

    memset(parser, 0, sizeof(*parser));

    resetResponse(&parser->response);
}

static HttpParseResult parseHeaders(HttpParser*              parser,
                                    const struct phr_header* headers,
                                    size_t                   headerCount) {
    if (parser == NULL || headers == NULL)
        return HTTP_PARSE_ERROR;

    if (headerCount > HTTP_MAX_HEADERS)
        return HTTP_PARSE_OVERFLOW;

    parser->response.headers.count = 0;

    for (size_t i = 0; i < headerCount; i++) {

        const struct phr_header* src = &headers[i];

        if (src->name == NULL)
            continue;

        if (src->name_len > HTTP_MAX_HEADER_NAME_LEN ||
            src->value_len > HTTP_MAX_HEADER_VALUE_LEN) {
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
                return HTTP_PARSE_ERROR;
            }

            if (contentLength > HTTP_MAX_BODY_SIZE)
                return HTTP_PARSE_OVERFLOW;

            if (parser->hasContentLength) {
                if (parser->contentLength != contentLength)
                    return HTTP_PARSE_ERROR;
            }

            parser->hasContentLength       = true;
            parser->contentLength          = contentLength;
            parser->response.contentLength = contentLength;
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
            if (!sliceEqualsIgnoreCase(encoding, "chunked"))
                return HTTP_PARSE_UNSUPPORTED;

            /*
             * Chunked must not be applied more than once.
             */
            if (parser->chunked)
                return HTTP_PARSE_ERROR;

            LOG_TRACE("parseHeaders(): chunked data detected.");

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
            }

            else if (sliceEqualsIgnoreCase(dst->value, "keep-alive")) {

                parser->response.connectionKeepAlive = true;
            }
        }
    }

    /*
     * Reject ambiguous response framing.
     */
    if (parser->chunked && parser->hasContentLength)
        return HTTP_PARSE_ERROR;

    return HTTP_PARSE_OK;
}

HttpParseResult httpParserParseResponse(HttpParser* parser, uint8_t* data,
                                        size_t len) {
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
        RETURN_VALUE_IF(
            (len < parser->parsedBytesPreviously || parser->headerBytes > len),
            true,
            ;, HTTP_PARSE_ERROR);

        parser->parsedBytesPreviously = len;

        if (parser->response.bodyForbidden) {
            parser->bodyReceived    = 0u;
            parser->messageComplete = true;
            return HTTP_PARSE_OK;
        }

        if (parser->chunked)
            return parseChunkedBody(parser, data, len);

        availableBody = len - parser->headerBytes;

        RETURN_VALUE_IF_GREATER(availableBody, HTTP_MAX_BODY_SIZE, ;
                                , HTTP_PARSE_OVERFLOW);

        if (parser->hasContentLength) {
            if (availableBody >= parser->contentLength) {
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

    RETURN_VALUE_IF((ret < 0 && len >= HTTP_MAX_HEADER_BYTES), true, ;
                    , HTTP_PARSE_OVERFLOW);

    RETURN_VALUE_IF(ret, -2, ;, HTTP_PARSE_INCOMPLETE);
    /*
     * Invalid HTTP response.
     */
    RETURN_VALUE_IF_LIITLE(ret, 0, ;, HTTP_PARSE_ERROR);

    RETURN_VALUE_IF_GREATER(ret, HTTP_MAX_HEADER_BYTES, ;, HTTP_PARSE_OVERFLOW);

    RETURN_VALUE_IF_GREATER(msgLen, HTTP_MAX_TARGET_LEN, ;
                            , HTTP_PARSE_OVERFLOW);

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

    HttpParseResult result = parseHeaders(parser, headers, headerCount);

    RETURN_VALUE_IF_NOT(result, HTTP_PARSE_OK, ;, result);

    /*
     * HTTP responses without a body.
     *
     * 1xx, 204 and 304 do not contain a message body.
     */
    if ((status >= 100 && status < 200) || status == 204 || status == 304) {

        parser->response.bodyForbidden = true;
        parser->messageComplete        = true;

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

        RETURN_VALUE_IF_GREATER(ret, len, ;, HTTP_PARSE_ERROR);

        availableBody = len - (size_t)ret;

        parser->bodyReceived = availableBody;

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

    return HTTP_PARSE_INCOMPLETE;
}

HttpParseResult httpParserFinish(HttpParser* parser) {
    if (parser == NULL)
        return HTTP_PARSE_ERROR;

    if (!parser->headerComplete)
        return HTTP_PARSE_ERROR;

    if (parser->messageComplete)
        return HTTP_PARSE_OK;

    /*
     * EOF is not a valid replacement for the terminal zero chunk.
     */
    if (parser->chunked)
        return HTTP_PARSE_ERROR;

    /*
     * EOF before Content-Length is a truncated response.
     */
    if (parser->hasContentLength) {
        if (parser->bodyReceived != parser->contentLength)
            return HTTP_PARSE_ERROR;

        parser->messageComplete = true;
        return HTTP_PARSE_OK;
    }

    if (parser->bodyReceived > HTTP_MAX_BODY_SIZE)
        return HTTP_PARSE_OVERFLOW;

    parser->messageComplete = true;
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

    LOG_TRACE("httpParserIsMessageComplete(): parser->messageComplete = %d",
              parser->messageComplete);

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