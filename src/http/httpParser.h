#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "httpParserCfg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {

    HTTP_STATE_HEADER,
    HTTP_STATE_BODY,
    HTTP_STATE_COMPLETE,
    HTTP_STATE_ERROR

} HttpParserState;

typedef enum {
    HTTP_PARSE_OK = 0,
    HTTP_PARSE_INCOMPLETE,
    HTTP_PARSE_ERROR,
    HTTP_PARSE_OVERFLOW,
    HTTP_PARSE_UNSUPPORTED
} HttpParseResult;

typedef enum { HTTP_VERSION_1_0 = 0, HTTP_VERSION_1_1 } HttpVersion;

typedef struct {
    const char* data;
    size_t      len;
} HttpSlice;

typedef struct {
    HttpSlice name;
    HttpSlice value;
} HttpHeader;

typedef struct {
    HttpHeader headers[HTTP_MAX_HEADERS];
    size_t     count;
} HttpHeaders;

typedef struct {
    HttpVersion version;

    int statusCode;

    HttpSlice reasonPhrase;

    HttpHeaders headers;

    /*
     * Number of bytes occupied by the HTTP header section.
     *
     * Example:
     *
     * HTTP/1.1 200 OK\r\n
     * Content-Length: 5\r\n
     * \r\n
     *
     * headerBytes == position immediately after the second CRLF.
     */
    size_t headerBytes;

    /*
     * Body information.
     */
    size_t contentLength;

    bool chunked;
    bool connectionClose;
    bool connectionKeepAlive;

    /*
     * True when the parser knows the response has no body.
     */
    bool bodyForbidden;

} HttpResponse;

typedef struct {
    /*
     * Number of bytes from the beginning of the current message
     * that were already supplied to the parser during the previous
     * parse attempt.
     *
     * This is used by picohttpparser's last_len parameter.
     */
    size_t parsedBytesPreviously;

    /*
     * Number of bytes consumed by the HTTP header.
     */
    size_t headerBytes;

    /*
     * Body length expected according to Content-Length.
     */
    size_t contentLength;

    bool hasContentLength;

    /*
     * Bytes of body already received.
     */
    size_t bodyReceived;

    // TODO:Critical: HTTP parser does not support repeated parse after header
    // completion

    bool headerComplete;
    bool messageComplete;

    bool chunked;
    bool connectionClose;

    HttpResponse response;

} HttpParser;

void httpParserInit(HttpParser* parser);

HttpParseResult httpParserParseResponse(HttpParser* parser, const uint8_t* data,
                                        size_t len);

bool httpParserIsHeaderComplete(const HttpParser* parser);

bool httpParserIsMessageComplete(const HttpParser* parser);

const HttpResponse* httpParserGetResponse(const HttpParser* parser);

const uint8_t* httpParserGetBody(const HttpParser* parser, const uint8_t* data,
                                 size_t len);

size_t httpParserGetBodyLength(const HttpParser* parser, size_t totalLen);

/*
 * Notify the parser that the transport reached a clean EOF.
 */
HttpParseResult httpParserFinish(HttpParser* parser);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_PARSER_H */