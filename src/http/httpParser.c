#include "httpParser.h"

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

            if (sliceEqualsIgnoreCase(dst->value, "chunked")) {

                parser->chunked          = true;
                parser->response.chunked = true;
            }
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

    return HTTP_PARSE_OK;
}

HttpParseResult httpParserParseResponse(HttpParser* parser, const uint8_t* data,
                                        size_t len) {
    if (parser == NULL || data == NULL)
        return HTTP_PARSE_ERROR;

    if (len == 0)
        return HTTP_PARSE_INCOMPLETE;

    /*
     * PicoHTTPParser writes pointers into the supplied buffer.
     * No HTTP data is copied here.
     */
    const char* method    = NULL;
    size_t      methodLen = 0;

    const char* path    = NULL;
    size_t      pathLen = 0;

    int minorVersion = 0;
    int status       = 0;

    const char* msg    = NULL;
    size_t      msgLen = 0;

    struct phr_header headers[HTTP_MAX_HEADERS];

    size_t headerCount = HTTP_MAX_HEADERS;

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
     * Incomplete HTTP response.
     */
    if (ret == -2)
        return HTTP_PARSE_INCOMPLETE;

    /*
     * Invalid HTTP response.
     */
    if (ret < 0)
        return HTTP_PARSE_ERROR;

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

    if (result != HTTP_PARSE_OK)
        return result;

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
    if (parser->chunked) {
        parser->messageComplete = false;
        return HTTP_PARSE_UNSUPPORTED;
    }

    /*
     * Content-Length response.
     */
    if (parser->hasContentLength) {

        size_t availableBody;

        if ((size_t)ret > len)
            return HTTP_PARSE_ERROR;

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
     * No Content-Length and not chunked.
     *
     * For HTTP/1.0/Connection: close responses the body is
     * delimited by connection close.
     *
     * We therefore cannot mark it complete until nth detects
     * socket closure.
     */
    if (parser->connectionClose || minorVersion == 0) {

        parser->messageComplete = false;
        return HTTP_PARSE_INCOMPLETE;
    }

    /*
     * HTTP/1.1 response without a body delimiter is not safe
     * to assume complete.
     */
    parser->messageComplete = false;

    return HTTP_PARSE_INCOMPLETE;
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

    if (parser->headerBytes > len)
        return NULL;

    return data + parser->headerBytes;
}

size_t httpParserGetBodyLength(const HttpParser* parser, size_t totalLen) {
    if (parser == NULL)
        return 0;

    if (!parser->headerComplete)
        return 0;

    if (parser->headerBytes > totalLen)
        return 0;

    size_t available = totalLen - parser->headerBytes;

    if (parser->contentLength < available)
        return parser->contentLength;

    return available;
}