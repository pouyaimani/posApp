#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "http.h"
#ifndef HTTP_CLIENT_NTH_HEADER
#define HTTP_CLIENT_NTH_HEADER "nth/nth.h"
#endif
#include HTTP_CLIENT_NTH_HEADER

typedef struct HttpClient HttpClient;

typedef enum {
    HTTP_CLIENT_OK      = 0, /* Start accepted, or complete HTTP response. */
    HTTP_CLIENT_INVALID = -1,
    HTTP_CLIENT_BUSY    = -2,
    HTTP_CLIENT_NO_TRANSACTION = -3,
    HTTP_CLIENT_HTTP_ERROR     = -4, /* Inspect status.http_result. */
    HTTP_CLIENT_NETWORK_ERROR  = -5, /* Inspect status.nth_result. */
    HTTP_CLIENT_TIMEOUT        = -6,
    HTTP_CLIENT_CANCELLED      = -7
} HttpClientResult;

typedef struct {
    HttpClientResult result;
    HttpResult       http_result;
    NthResult        nth_result;
    unsigned         http_status; /* 0 until a response status is parsed. */
    uint64_t         body_bytes;
} HttpClientStatus;

/* Called once for a successfully started operation, including cancel/failure.
 * NOT called for a synchronous start rejection. Status is valid until the next
 * start. Queue the next operation; do not start/reset/cancel from callbacks.
 * HTTP_CLIENT_OK means complete HTTP framing, not HTTP 2xx or JSON success. */
typedef void (*HttpClientDone)(HttpClient*             client,
                               const HttpClientStatus* status, void* user);

typedef struct {
    const char* connect_host; /* NTH destination: no scheme or :port. */
    uint16_t    port;
    uint32_t    timeout_ms; /* Per NTH phase; retries have own connect timer. */
    HttpRequest request;    /* request.host is the HTTP authority. */
    const uint8_t*    body;
    size_t            body_length; /* Must equal request.body_length. */
    HttpParserOptions parser_options;
    HttpCallbacks     callbacks; /* Optional headers/body callbacks. */
    HttpClientDone    on_done;
    void*             user;
} HttpClientRequest;

/* Zero-initialize before the FIRST use: static HttpClient client;
 * Use from the same task as nth.tick; no synchronization is provided.
 * Context and header storage must remain alive until completion/cancel.
 * Request strings/body are copied during start and may be released afterwards.
 * Do not overlap request input/header storage with NTH's transaction buffers.
 * Read status/busy, but do not modify other fields while active. */
struct HttpClient {
    HttpParser       parser;
    NthTransaction*  tx;
    HttpClientStatus status;
    HttpCallbacks    callbacks;
    HttpClientDone   on_done;
    void*            user;
    bool             busy;
    bool             starting;
    bool             in_callback;
};

/* The supplied NTH transport is TCP, not TLS. This adapter is plain HTTP.
 * Sending to port 443 does not enable TLS. A TLS transport is a separate step.
 * Entire request (headers + small body) must fit tx->txBuffer.capacity.
 * Response is streamed and need not fit the NTH buffer.
 * Connection: close is forced because this NTH closes after each response. */
HttpClientResult http_client_start(HttpClient*              client,
                                   const HttpClientRequest* request,
                                   char*                    header_storage,
                                   size_t                   header_capacity);

/* Cancel from the owning task outside callbacks. Closes/releases NTH and calls
 * on_done once. Already idle returns OK without another callback. */
HttpClientResult http_client_cancel(HttpClient* client);

#endif
