#include "httpParser.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_CR '\r'
#define HTTP_LF '\n'

static bool isTokenChar(char c) {
    return isalnum((unsigned char)c) || c == '!' || c == '#' || c == '$' ||
           c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' ||
           c == '-' || c == '.' || c == '^' || c == '_' || c == '`' ||
           c == '|' || c == '~';
}

static bool stringEqualsIgnoreCase(const char* a, const char* b) {
    if (!a || !b)
        return false;

    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }

        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static bool startsWithIgnoreCase(const char* value, const char* prefix) {
    while (*prefix) {
        if (*value == '\0')
            return false;

        if (tolower((unsigned char)*value) != tolower((unsigned char)*prefix)) {
            return false;
        }

        ++value;
        ++prefix;
    }

    return true;
}

static char* trimWhitespace(char* str) {
    char* end;

    while (*str == ' ' || *str == '\t')
        ++str;

    end = str + strlen(str);

    while (end > str && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
        *end = '\0';
    }

    return str;
}

static bool findCrlf(const uint8_t* data, size_t len, size_t start,
                     size_t* lineEnd) {
    if (!data || !lineEnd || start >= len)
        return false;

    for (size_t i = start; i + 1 < len; ++i) {
        if (data[i] == HTTP_CR && data[i + 1] == HTTP_LF) {
            *lineEnd = i;
            return true;
        }
    }

    return false;
}

static bool copyLine(const uint8_t* data, size_t start, size_t end, char* dst,
                     size_t dstSize) {
    size_t len;

    if (!data || !dst || dstSize == 0 || end < start)
        return false;

    len = end - start;

    if (len >= dstSize)
        return false;

    memcpy(dst, data + start, len);
    dst[len] = '\0';

    return true;
}

static bool parseStatusLine(HttpParser* parser, const char* line) {
    const char*   p;
    const char*   space;
    char*         endPtr;
    unsigned long status;

    if (!parser || !line)
        return false;

    if (!startsWithIgnoreCase(line, "HTTP/"))
        return false;

    p = line + 5;

    if (!isdigit((unsigned char)p[0]) || p[1] != '.' ||
        !isdigit((unsigned char)p[2])) {
        return false;
    }

    if (p[3] != ' ')
        return false;

    parser->httpVersion[0] = '1';
    parser->httpVersion[1] = '.';
    parser->httpVersion[2] = p[2];
    parser->httpVersion[3] = '\0';

    p += 4;

    if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1]) ||
        !isdigit((unsigned char)p[2])) {
        return false;
    }

    status = strtoul(p, &endPtr, 10);

    if (status < 100 || status > 599)
        return false;

    parser->statusCode = (uint16_t)status;

    if (*endPtr != ' ' && *endPtr != '\0')
        return false;

    space = endPtr;

    if (*space == ' ')
        ++space;

    if (strlen(space) >= HTTP_MAX_REASON_PHRASE)
        return false;

    snprintf(parser->reasonPhrase, sizeof(parser->reasonPhrase), "%s", space);

    return true;
}

static bool parseContentLength(const char* value, size_t* result) {
    char*              end;
    unsigned long long parsed;

    if (!value || !result || *value == '\0')
        return false;

    while (*value == ' ' || *value == '\t')
        ++value;

    errno = 0;

    parsed = strtoull(value, &end, 10);

    if (errno == ERANGE || parsed > SIZE_MAX) {
        return false;
    }

    while (*end == ' ' || *end == '\t')
        ++end;

    if (*end != '\0')
        return false;

    *result = (size_t)parsed;

    return true;
}

static bool parseTransferEncoding(const char* value, bool* chunked) {
    char   token[32];
    size_t tokenLen = 0;

    if (!value || !chunked)
        return false;

    *chunked = false;

    while (*value) {

        while (*value == ' ' || *value == '\t' || *value == ',') {
            ++value;
        }

        if (*value == '\0')
            break;

        tokenLen = 0;

        while (*value && *value != ',' && *value != ';') {

            if (tokenLen + 1 >= sizeof(token))
                return false;

            token[tokenLen++] = (char)tolower((unsigned char)*value);

            ++value;
        }

        token[tokenLen] = '\0';

        while (*value && *value != ',') {
            ++value;
        }

        if (tokenLen == 0)
            continue;

        if (strcmp(token, "chunked") == 0) {
            *chunked = true;
        } else if (strcmp(token, "identity") == 0) {
            /* supported */
        } else {
            return false;
        }

        if (*value == ',')
            ++value;
    }

    return true;
}

static bool parseHeaders(HttpParser* parser, const uint8_t* data, size_t start,
                         size_t end) {
    size_t offset = start;

    while (offset < end) {

        size_t lineEnd;

        if (!findCrlf(data, end, offset, &lineEnd))
            return false;

        if (lineEnd == offset) {
            parser->headerEnd = lineEnd + 2;
            return true;
        }

        if (parser->headerCount >= HTTP_MAX_HEADERS) {
            parser->error = HTTP_PARSE_ERR_TOO_MANY_HEADERS;
            return false;
        }

        size_t lineLen = lineEnd - offset;

        if (lineLen >= 256) {
            parser->error = HTTP_PARSE_ERR_HEADER_TOO_LONG;
            return false;
        }

        char line[256];

        if (!copyLine(data, offset, lineEnd, line, sizeof(line))) {
            parser->error = HTTP_PARSE_ERR_HEADER_TOO_LONG;
            return false;
        }

        char* colon = strchr(line, ':');

        if (!colon) {
            parser->error = HTTP_PARSE_ERR_INVALID_HEADER;
            return false;
        }

        *colon = '\0';

        char* name  = line;
        char* value = trimWhitespace(colon + 1);

        if (*name == '\0')
            return false;

        for (char* p = name; *p; ++p) {
            if (!isTokenChar(*p)) {
                parser->error = HTTP_PARSE_ERR_INVALID_HEADER;
                return false;
            }
        }

        if (strlen(name) >= HTTP_MAX_HEADER_NAME ||
            strlen(value) >= HTTP_MAX_HEADER_VALUE) {
            parser->error = HTTP_PARSE_ERR_HEADER_TOO_LONG;
            return false;
        }

        HttpParsedHeader* header = &parser->headers[parser->headerCount];

        snprintf(header->name, sizeof(header->name), "%s", name);

        snprintf(header->value, sizeof(header->value), "%s", value);

        ++parser->headerCount;

        offset = lineEnd + 2;
    }

    return false;
}

static void determineBodyFraming(HttpParser* parser) {
    const HttpParsedHeader* header;

    /*
     * RFC semantics:
     *
     * 1xx, 204, 304 have no message body.
     */
    if ((parser->statusCode >= 100 && parser->statusCode <= 199) ||
        parser->statusCode == 204 || parser->statusCode == 304) {
        parser->noBody = true;
        return;
    }

    header = httpParserGetHeader(parser, "Transfer-Encoding");

    if (header) {
        bool chunked;

        if (!parseTransferEncoding(header->value, &chunked)) {
            parser->error = HTTP_PARSE_ERR_UNSUPPORTED_TRANSFER_ENCODING;
            return;
        }

        parser->chunked = chunked;
    }

    header = httpParserGetHeader(parser, "Content-Length");

    if (header) {
        size_t contentLength;

        if (!parseContentLength(header->value, &contentLength)) {
            parser->error = HTTP_PARSE_ERR_INVALID_CONTENT_LENGTH;
            return;
        }

        parser->contentLength    = contentLength;
        parser->hasContentLength = true;
    }

    /*
     * Transfer-Encoding takes precedence over
     * Content-Length.
     *
     * We don't accept conflicting framing where
     * Content-Length is present with chunked data.
     */
    if (parser->chunked && parser->hasContentLength) {
        parser->error = HTTP_PARSE_ERR_CONFLICTING_LENGTH;
        return;
    }

    header = httpParserGetHeader(parser, "Connection");

    if (header && strstr(header->value, "close") != NULL) {
        parser->connectionClose = true;
    }

    /*
     * With the exact NTH API we have no EOF notification.
     *
     * Therefore a response whose body is delimited only
     * by connection close cannot be represented safely.
     */
    if (!parser->chunked && !parser->hasContentLength) {
        parser->noBody = true;
    }
}

static HttpParseResult parserError(HttpParser* parser, HttpParseResult error) {
    parser->state = HTTP_PARSE_ERROR;
    parser->error = error;
    return error;
}

void httpParserInit(HttpParser* parser) {
    if (!parser)
        return;

    memset(parser, 0, sizeof(*parser));

    parser->state = HTTP_PARSE_STATUS_LINE;
}

HttpParseResult httpParserFeed(HttpParser* parser, const uint8_t* data,
                               size_t len) {
    size_t lineEnd;

    if (!parser || (!data && len != 0))
        return HTTP_PARSE_ERR_INVALID_ARGUMENT;

    if (parser->state == HTTP_PARSE_COMPLETE)
        return HTTP_PARSE_COMPLETE_RESULT;

    if (parser->state == HTTP_PARSE_ERROR)
        return (HttpParseResult)parser->error;

    /*
     * STATUS LINE
     */
    if (parser->state == HTTP_PARSE_STATUS_LINE) {

        if (!findCrlf(data, len, parser->scanOffset, &lineEnd)) {

            parser->scanOffset = len;
            return HTTP_PARSE_INCOMPLETE;
        }

        char line[128];

        if (!copyLine(data, 0, lineEnd, line, sizeof(line))) {
            return parserError(parser, HTTP_PARSE_ERR_INVALID_STATUS_LINE);
        }

        if (!parseStatusLine(parser, line)) {
            return parserError(parser, HTTP_PARSE_ERR_INVALID_STATUS_LINE);
        }

        parser->scanOffset = lineEnd + 2;
        parser->state      = HTTP_PARSE_HEADERS;
    }

    /*
     * HEADERS
     */
    if (parser->state == HTTP_PARSE_HEADERS) {

        size_t offset = parser->scanOffset;

        while (true) {

            if (!findCrlf(data, len, offset, &lineEnd)) {

                parser->scanOffset = len;
                return HTTP_PARSE_INCOMPLETE;
            }

            if (lineEnd == offset) {
                parser->headerEnd  = lineEnd + 2;
                parser->bodyOffset = parser->headerEnd;

                /*
                 * Parse all headers from the beginning of
                 * the header section.
                 */
                if (!parseHeaders(parser, data, parser->scanOffset,
                                  parser->headerEnd)) {

                    if (parser->error != 0)
                        return parserError(parser,
                                           (HttpParseResult)parser->error);

                    return parserError(parser, HTTP_PARSE_ERR_INVALID_HEADER);
                }

                determineBodyFraming(parser);

                if (parser->error != 0) {
                    return parserError(parser, (HttpParseResult)parser->error);
                }

                if (parser->noBody) {
                    parser->state = HTTP_PARSE_COMPLETE;
                    return HTTP_PARSE_COMPLETE_RESULT;
                }

                if (parser->chunked) {
                    parser->state = HTTP_PARSE_CHUNK_SIZE;
                } else {
                    parser->state = HTTP_PARSE_BODY;
                }

                break;
            }

            offset = lineEnd + 2;
        }
    }

    /*
     * CONTENT-LENGTH BODY
     */
    if (parser->state == HTTP_PARSE_BODY) {

        size_t available;

        if (len < parser->bodyOffset)
            return parserError(parser, HTTP_PARSE_ERR_INVALID_HEADER);

        available = len - parser->bodyOffset;

        if (available >= parser->contentLength) {
            parser->bodyReceived = parser->contentLength;

            parser->state = HTTP_PARSE_COMPLETE;

            return HTTP_PARSE_COMPLETE_RESULT;
        }

        parser->bodyReceived = available;

        return HTTP_PARSE_INCOMPLETE;
    }

    /*
     * CHUNKED BODY
     *
     * We only determine completion here.
     * Actual decoding is performed by
     * httpParserParseComplete().
     */
    if (parser->state == HTTP_PARSE_CHUNK_SIZE ||
        parser->state == HTTP_PARSE_CHUNK_DATA ||
        parser->state == HTTP_PARSE_CHUNK_CRLF ||
        parser->state == HTTP_PARSE_CHUNK_TRAILERS) {

        /*
         * The final chunk can only be identified by parsing
         * the chunk stream.
         *
         * Do that using the complete-buffer parser.
         */
        HttpParser temp = *parser;

        HttpParseResult result =
            httpParserParseComplete(&temp, (uint8_t*)data, len);

        if (result == HTTP_PARSE_COMPLETE_RESULT) {
            *parser = temp;
            return result;
        }

        if (result < 0) {
            *parser = temp;
            return result;
        }

        return HTTP_PARSE_INCOMPLETE;
    }

    return HTTP_PARSE_INCOMPLETE;
}

static bool parseHexSize(const char* str, size_t* result) {
    size_t value     = 0;
    bool   digitSeen = false;

    while (*str) {

        char c = *str;

        if (c == ';')
            break;

        if (c == ' ' || c == '\t') {
            ++str;
            continue;
        }

        unsigned int digit;

        if (c >= '0' && c <= '9')
            digit = (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = (unsigned int)(c - 'A' + 10);
        else
            return false;

        digitSeen = true;

        if (value > (SIZE_MAX - digit) / 16)
            return false;

        value = value * 16 + digit;

        ++str;
    }

    if (!digitSeen)
        return false;

    *result = value;
    return true;
}

static HttpParseResult parseChunkedComplete(HttpParser* parser, uint8_t* data,
                                            size_t len) {
    size_t pos     = parser->bodyOffset;
    size_t decoded = 0;

    while (true) {

        size_t lineEnd;

        if (!findCrlf(data, len, pos, &lineEnd)) {
            return HTTP_PARSE_INCOMPLETE;
        }

        size_t lineLen = lineEnd - pos;

        if (lineLen >= 128) {
            return parserError(parser, HTTP_PARSE_ERR_INVALID_CHUNK_SIZE);
        }

        char line[128];

        memcpy(line, data + pos, lineLen);
        line[lineLen] = '\0';

        size_t chunkSize;

        if (!parseHexSize(line, &chunkSize)) {
            return parserError(parser, HTTP_PARSE_ERR_INVALID_CHUNK_SIZE);
        }

        pos = lineEnd + 2;

        if (chunkSize == 0) {

            /*
             * Parse trailers.
             *
             * Empty line immediately after zero chunk
             * means no trailers.
             */
            while (true) {

                if (!findCrlf(data, len, pos, &lineEnd)) {
                    return HTTP_PARSE_INCOMPLETE;
                }

                if (lineEnd == pos) {
                    parser->bodyReceived = decoded;
                    parser->state        = HTTP_PARSE_COMPLETE;
                    return HTTP_PARSE_COMPLETE_RESULT;
                }

                /*
                 * Validate trailer syntax.
                 */
                size_t trailerLen = lineEnd - pos;

                if (trailerLen >= 256)
                    return parserError(parser, HTTP_PARSE_ERR_INVALID_HEADER);

                char trailer[256];

                memcpy(trailer, data + pos, trailerLen);

                trailer[trailerLen] = '\0';

                if (!strchr(trailer, ':'))
                    return parserError(parser, HTTP_PARSE_ERR_INVALID_HEADER);

                pos = lineEnd + 2;
            }
        }

        if (chunkSize > SIZE_MAX - decoded)
            return parserError(parser, HTTP_PARSE_ERR_BODY_TOO_LARGE);

        if (decoded + chunkSize > len - parser->bodyOffset) {
            return HTTP_PARSE_INCOMPLETE;
        }

        if (chunkSize > len - pos) {
            return HTTP_PARSE_INCOMPLETE;
        }

        /*
         * Need chunk data plus CRLF.
         */
        if (len - pos < chunkSize + 2)
            return HTTP_PARSE_INCOMPLETE;

        if (data[pos + chunkSize] != HTTP_CR ||
            data[pos + chunkSize + 1] != HTTP_LF) {
            return parserError(parser, HTTP_PARSE_ERR_INVALID_CHUNK_DATA);
        }

        /*
         * Decode in-place.
         *
         * Destination is always <= source, so this is safe.
         */
        if (chunkSize != 0) {
            memmove(data + parser->bodyOffset + decoded, data + pos, chunkSize);
        }

        decoded += chunkSize;
        pos += chunkSize + 2;
    }
}

HttpParseResult httpParserParseComplete(HttpParser* parser, uint8_t* data,
                                        size_t len) {
    if (!parser || !data)
        return HTTP_PARSE_ERR_INVALID_ARGUMENT;

    /*
     * First parse status + headers normally.
     */
    if (parser->state == HTTP_PARSE_STATUS_LINE ||
        parser->state == HTTP_PARSE_HEADERS) {

        HttpParseResult result = httpParserFeed(parser, data, len);

        if (result < 0)
            return result;

        if (parser->state == HTTP_PARSE_COMPLETE)
            return HTTP_PARSE_COMPLETE_RESULT;
    }

    if (parser->state == HTTP_PARSE_BODY) {

        if (len < parser->bodyOffset)
            return parserError(parser, HTTP_PARSE_ERR_INVALID_HEADER);

        if (len - parser->bodyOffset < parser->contentLength) {
            return HTTP_PARSE_INCOMPLETE;
        }

        parser->bodyReceived = parser->contentLength;

        parser->state = HTTP_PARSE_COMPLETE;

        return HTTP_PARSE_COMPLETE_RESULT;
    }

    if (parser->state == HTTP_PARSE_CHUNK_SIZE ||
        parser->state == HTTP_PARSE_CHUNK_DATA ||
        parser->state == HTTP_PARSE_CHUNK_CRLF ||
        parser->state == HTTP_PARSE_CHUNK_TRAILERS) {

        return parseChunkedComplete(parser, data, len);
    }

    if (parser->state == HTTP_PARSE_COMPLETE)
        return HTTP_PARSE_COMPLETE_RESULT;

    return HTTP_PARSE_INCOMPLETE;
}

const HttpParsedHeader* httpParserGetHeader(const HttpParser* parser,
                                            const char*       name) {
    if (!parser || !name)
        return NULL;

    for (size_t i = 0; i < parser->headerCount; ++i) {

        if (stringEqualsIgnoreCase(parser->headers[i].name, name)) {
            return &parser->headers[i];
        }
    }

    return NULL;
}

bool httpParserIsComplete(const HttpParser* parser) {
    return parser && parser->state == HTTP_PARSE_COMPLETE;
}