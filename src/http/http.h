#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nth/nth.h"
#include "httpParser.h"

#define HTTP_MAX_REQUEST_HEADERS 12

typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE
} HttpMethod;

typedef enum { HTTP_VERSION_1_0 = 0, HTTP_VERSION_1_1 } HttpVersion;

typedef enum {
    HTTP_STATE_IDLE = 0,
    HTTP_STATE_CONNECTING,
    HTTP_STATE_SENDING,
    HTTP_STATE_RECEIVING,
    HTTP_STATE_COMPLETED,
    HTTP_STATE_FAILED
} HttpState;

typedef enum {
    HTTP_OK = 0,

    HTTP_ERR_INVALID_ARG,
    HTTP_ERR_BUSY,
    HTTP_ERR_NTH_ALLOC,
    HTTP_ERR_REQUEST_TOO_LARGE,
    HTTP_ERR_INVALID_HOST,
    HTTP_ERR_REQUEST_BUILD,
    HTTP_ERR_NTH_CONNECT,
    HTTP_ERR_NTH_SEND,
    HTTP_ERR_NTH_RECEIVE,
    HTTP_ERR_TIMEOUT,

    HTTP_ERR_PARSE,
    HTTP_ERR_RESPONSE_TOO_LARGE,
    HTTP_ERR_UNSUPPORTED_RESPONSE,

    HTTP_ERR_STATUS
} HttpResult;

typedef struct {
    const char* name;
    const char* value;
} HttpRequestHeader;

typedef struct {
    HttpMethod method;

    const char* host;
    uint16_t    port;

    const char* path;

    HttpVersion version;

    HttpRequestHeader headers[HTTP_MAX_REQUEST_HEADERS];
    size_t            headerCount;

    const uint8_t* body;
    size_t         bodyLen;
} HttpRequest;

typedef struct {
    uint16_t    statusCode;
    const char* reasonPhrase;

    const HttpParsedHeader* headers;
    size_t                  headerCount;

    const uint8_t* body;
    size_t         bodyLen;
} HttpResponse;

struct Http;

typedef void (*HttpCompleteCallback)(struct Http*        http,
                                     const HttpResponse* response,
                                     void*               userData);

typedef void (*HttpErrorCallback)(struct Http* http, HttpResult error,
                                  void* userData);

typedef struct Http {
    NthTransaction* tx;

    HttpRequest request;

    HttpResponse response;
    HttpParser   parser;

    HttpState  state;
    HttpResult error;

    HttpCompleteCallback onComplete;
    HttpErrorCallback    onError;

    void* userData;
} Http;

void httpInit(Http* http);

void httpRelease(Http* http);

HttpResult httpRequest(Http* http, const HttpRequest* request);

HttpResult httpGet(Http* http, const char* host, uint16_t port,
                   const char* path);

HttpResult httpPost(Http* http, const char* host, uint16_t port,
                    const char* path, const char* contentType,
                    const uint8_t* body, size_t bodyLen);

void httpSetCallbacks(Http* http, HttpCompleteCallback onComplete,
                      HttpErrorCallback onError, void* userData);

void httpTick(Http* http);

void httpAbort(Http* http);

bool httpIsBusy(const Http* http);

const HttpResponse* httpGetResponse(const Http* http);

const HttpParsedHeader* httpGetResponseHeader(const Http* http,
                                              const char* name);

const char* httpGetErrorString(HttpResult error);

#endif