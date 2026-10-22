#include "tmsTimeSync.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>

static HttpClient* probe;
static char*       header_storage;
static char*       response_body;
static size_t      body_length;

static int probeHeaders(void* user, const HttpResponse* response,
                        const struct phr_header* headers, size_t count) {
    HttpView encoding;
    (void)user;
    LOG_TRACE("HTTP probe: status=%u", response->status);
    if (response->status < 200)
        return 0;
    if (http_find_header(headers, count, "Content-Encoding", &encoding) &&
        !http_view_equal_ci(encoding, "identity"))
        return -1;
    return 0;
}

static int probeBody(void* user, const uint8_t* bytes, size_t length) {
    (void)user;
    if (length > 512 - 1 - body_length)
        return -1;
    memcpy(response_body + body_length, bytes, length);
    body_length += length;
    response_body[body_length] = '\0';
    return 0;
}

static void probeDone(HttpClient* client, const HttpClientStatus* status,
                      void* user) {
    (void)client;
    (void)user;
    LOG_TRACE("HTTP probe: result=%d http=%u parser=%d nth=%d body=%lu",
              (int)status->result, status->http_status,
              (int)status->http_result, (int)status->nth_result,
              (unsigned long)body_length);
    if (status->result == HTTP_CLIENT_OK) {
        LOG_TRACE("HTTP probe body: %s", response_body);
        /* Next step: parse JSON and validate application code/data.ts.
         * A complete HTTP response alone does not mean the API succeeded. */
    }
}

HttpClientResult tmsHttpProbeStart(const char* host, uint16_t port) {
    HttpClientRequest request;
    probe                = MEM_ALLOC(sizeof(HttpClient));
    HttpHeader headers[] = {
        {HTTP_VIEW_LITERAL("Accept"), HTTP_VIEW_LITERAL("application/json")},
        {HTTP_VIEW_LITERAL("Accept-Encoding"), HTTP_VIEW_LITERAL("identity")}};
    char authority[NTH_MAX_HOST_LEN + 8];
    int  n;
    header_storage = MEM_ALLOC(1024);
    response_body  = MEM_ALLOC(512);
    HttpClientResult result;
    if (probe->busy || probe->in_callback)
        return HTTP_CLIENT_BUSY;
    if (!host || !port)
        return HTTP_CLIENT_INVALID;
    n = snprintf(authority, sizeof(authority), "%s:%u", host, (unsigned)port);
    if (n < 0 || (size_t)n >= sizeof(authority))
        return HTTP_CLIENT_INVALID;
    memset(&request, 0, sizeof(request));
    body_length                  = 0;
    response_body[0]             = '\0';
    request.connect_host         = host;
    request.port                 = port;
    request.timeout_ms           = 15000;
    request.request.method       = HTTP_GET;
    request.request.target.ptr   = "/api/ntp/sync";
    request.request.target.len   = sizeof("/api/ntp/sync") - 1;
    request.request.host.ptr     = authority;
    request.request.host.len     = (size_t)n;
    request.request.headers      = headers;
    request.request.header_count = sizeof(headers) / sizeof(headers[0]);
    request.parser_options       = http_parser_options();
    request.parser_options.max_body_bytes = 512 - 1;
    request.parser_options.max_wire_bytes = 16384;
    request.callbacks.on_headers          = probeHeaders;
    request.callbacks.on_body             = probeBody;
    request.on_done                       = probeDone;
    result = http_client_start(probe, &request, header_storage, 1024);
    if (result != HTTP_CLIENT_OK) {
        LOG_TRACE("HTTP start rejected: result=%d parser=%d (%s)", (int)result,
                  (int)probe->status.http_result,
                  http_result_string(probe->status.http_result));

        LOG_TRACE("HTTP buffers: NTH=%lu header=%lu",
                  (unsigned long)NT_IO_BUFFER_SIZE, (unsigned long)1024);
    }
    return result;
}

HttpClientResult tmsHttpProbeCancel(void) { return http_client_cancel(&probe); }
