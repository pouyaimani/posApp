#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTP_MAX_HEADERS       16
#define HTTP_MAX_HEADER_NAME   32
#define HTTP_MAX_HEADER_VALUE  128
#define HTTP_MAX_REASON_PHRASE 32

typedef enum {
    HTTP_PARSE_STATUS_LINE = 0,
    HTTP_PARSE_HEADERS,
    HTTP_PARSE_BODY,
    HTTP_PARSE_CHUNK_SIZE,
    HTTP_PARSE_CHUNK_DATA,
    HTTP_PARSE_CHUNK_CRLF,
    HTTP_PARSE_CHUNK_TRAILERS,
    HTTP_PARSE_COMPLETE,
    HTTP_PARSE_ERROR
} HttpParseState;

typedef enum {
    HTTP_PARSE_OK = 0,
    HTTP_PARSE_INCOMPLETE,
    HTTP_PARSE_COMPLETE_RESULT,

    HTTP_PARSE_ERR_INVALID_ARGUMENT,
    HTTP_PARSE_ERR_INVALID_STATUS_LINE,
    HTTP_PARSE_ERR_INVALID_HEADER,
    HTTP_PARSE_ERR_TOO_MANY_HEADERS,
    HTTP_PARSE_ERR_HEADER_TOO_LONG,
    HTTP_PARSE_ERR_INVALID_CONTENT_LENGTH,
    HTTP_PARSE_ERR_CONFLICTING_LENGTH,
    HTTP_PARSE_ERR_UNSUPPORTED_TRANSFER_ENCODING,
    HTTP_PARSE_ERR_INVALID_CHUNK_SIZE,
    HTTP_PARSE_ERR_INVALID_CHUNK_DATA,
    HTTP_PARSE_ERR_BODY_TOO_LARGE,
    HTTP_PARSE_ERR_UNSUPPORTED_RESPONSE
} HttpParseResult;

typedef struct {
    char name[HTTP_MAX_HEADER_NAME];
    char value[HTTP_MAX_HEADER_VALUE];
} HttpParsedHeader;

typedef struct {
    HttpParseState state;

    size_t scanOffset;

    size_t headerEnd;
    size_t bodyOffset;

    size_t contentLength;
    size_t bodyReceived;

    size_t chunkSize;
    size_t chunkReceived;

    bool hasContentLength;
    bool chunked;
    bool connectionClose;

    bool noBody;

    uint16_t statusCode;

    char httpVersion[9];
    char reasonPhrase[HTTP_MAX_REASON_PHRASE];

    HttpParsedHeader headers[HTTP_MAX_HEADERS];
    size_t           headerCount;

    int error;
} HttpParser;

void httpParserInit(HttpParser* parser);

HttpParseResult httpParserFeed(HttpParser* parser, const uint8_t* data,
                               size_t len);

HttpParseResult httpParserParseComplete(HttpParser* parser, uint8_t* data,
                                        size_t len);

const HttpParsedHeader* httpParserGetHeader(const HttpParser* parser,
                                            const char*       name);

bool httpParserIsComplete(const HttpParser* parser);

#endif