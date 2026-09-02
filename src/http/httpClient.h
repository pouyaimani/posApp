#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "httpParser.h"
#include "nth/nth.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HTTP_CLIENT_IDLE = 0,
    HTTP_CLIENT_REQUEST_READY,
    HTTP_CLIENT_REQUESTING,
    HTTP_CLIENT_RESPONSE_RECEIVING,
    HTTP_CLIENT_COMPLETED,
    HTTP_CLIENT_FAILED
} HttpClientState;

typedef enum {
    HTTP_CLIENT_OK = 0,
    HTTP_CLIENT_ERR_INVALID_ARG,
    HTTP_CLIENT_ERR_REQUEST_TOO_LARGE,
    HTTP_CLIENT_ERR_PARSE,
    HTTP_CLIENT_ERR_HTTP_STATUS,
    HTTP_CLIENT_ERR_NTH
} HttpClientResult;

typedef struct HttpClient HttpClient;

typedef void (*HttpClientCallback)(HttpClient* client, void* userData);

typedef struct {
    const char* name;
    const char* value;
} HttpRequestHeader;

struct HttpClient {

    /*
     * Underlying Nth transaction.
     *
     * Owned by HttpClient.
     */
    NthTransaction* tx;

    /*
     * HTTP parser state.
     */
    HttpParser parser;

    /*
     * HTTP client state.
     */
    HttpClientState state;

    /*
     * Application-defined context.
     */
    void* userData;

    /*
     * HTTP callbacks.
     */
    HttpClientCallback onComplete;
    HttpClientCallback onFailure;

    /*
     * Last HTTP parser error.
     */
    HttpParseResult parseResult;

    /*
     * Last Nth error.
     */
    NthResult nthResult;
};

/**
 * Initialize an HTTP client around an existing Nth transaction.
 */
void httpClientInit(HttpClient* client, NthTransaction* tx, void* userData);

/**
 * Prepare a HTTP GET request.
 */
HttpClientResult httpClientBuildGet(HttpClient* client, const char* path,
                                    const char*              host,
                                    const HttpRequestHeader* headers,
                                    size_t                   headerCount);

/**
 * Prepare a HTTP POST request.
 *
 * Body is copied into Nth's TX buffer.
 */
HttpClientResult httpClientBuildPost(HttpClient* client, const char* path,
                                     const char*              host,
                                     const HttpRequestHeader* headers,
                                     size_t headerCount, const uint8_t* body,
                                     size_t bodyLen);

/**
 * Send the already prepared request.
 */
HttpClientResult httpClientSend(HttpClient* client);

/**
 * Get parsed HTTP response.
 */
const HttpResponse* httpClientGetResponse(const HttpClient* client);

/**
 * Get response body.
 *
 * Pointer refers directly to Nth's RX buffer.
 */
const uint8_t* httpClientGetBody(const HttpClient* client);

/**
 * Get response body length.
 */
size_t httpClientGetBodyLength(const HttpClient* client);

/**
 * Check whether response is complete.
 */
bool httpClientIsComplete(const HttpClient* client);

/**
 * Check whether HTTP client failed.
 */
bool httpClientIsFailed(const HttpClient* client);

#ifdef __cplusplus
}
#endif

#endif