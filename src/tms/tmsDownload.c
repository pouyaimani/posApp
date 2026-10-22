#include "tmsDownload.h"
#include <string.h>
#include <limits.h>

static TmsDownloadResult parse_url(TmsDownload* d, const char* url) {
    const char *a, *end, *colon, *p;
    size_t      n, hostlen;
    unsigned    port = 80;
    if (!strncmp(url, "https://", 8))
        return TMS_DL_HTTPS_UNSUPPORTED;
    if (strncmp(url, "http://", 7)) {
        LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
        return TMS_DL_URL;
    }
    for (p = url; *p; ++p)
        if ((unsigned char)*p <= 32 || (unsigned char)*p >= 127 || *p == '#' ||
            *p == '\\') {
            LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
            return TMS_DL_URL;
        }
    a   = url + 7;
    end = a + strcspn(a, "/?");
    n   = (size_t)(end - a);
    if (!n || n >= sizeof(d->authority)) {
        LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
        return TMS_DL_URL;
    }
    colon   = memchr(a, ':', n);
    hostlen = colon ? (size_t)(colon - a) : n;
    if (!hostlen || hostlen >= sizeof(d->host)) {
        LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
        return TMS_DL_URL;
    }
    for (p = a; p < a + hostlen; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) {
            LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
            return TMS_DL_URL;
        }
    if (colon) {
        port = 0;
        if (colon + 1 == end) {
            LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
            return TMS_DL_URL;
        }
        for (p = colon + 1; p < end; ++p) {
            if (*p < '0' || *p > '9') {
                LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
                return TMS_DL_URL;
            }
            port = port * 10 + (unsigned)(*p - '0');
            if (port > 65535) {
                LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
                return TMS_DL_URL;
            }
        }
        if (!port) {
            LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
            return TMS_DL_URL;
        }
    }
    memcpy(d->host, a, hostlen);
    d->host[hostlen] = 0;
    memcpy(d->authority, a, n);
    d->authority[n] = 0;
    d->port         = (uint16_t)port;
    n               = strlen(end);
    if (n + (*end == '/' ? 0u : 1u) >= sizeof(d->target)) {
        LOG_TRACE("TMS URL rejected at line=%d", __LINE__);
        return TMS_DL_URL;
    }
    if (*end == '/')
        memcpy(d->target, end, n + 1);
    else {
        d->target[0] = '/';
        memcpy(d->target + 1, end, n + 1);
    }
    return TMS_DL_OK;
}
static int fail(TmsDownload* d, TmsDownloadResult result) {
    d->status.result = result;
    return -1;
}
static int headers(void* user, const HttpResponse* r,
                   const struct phr_header* h, size_t count) {
    TmsDownload* d = user;
    size_t       i;
    if (r->status < 200)
        return 0;
    if (r->status != 200)
        return fail(d, TMS_DL_HTTP);
    if (r->has_content_length && r->content_length != d->status.expected)
        return fail(d, TMS_DL_SIZE);
    for (i = 0; i < count; ++i) {
        HttpView name  = {h[i].name, h[i].name_len},
                 value = {h[i].value, h[i].value_len};
        if (http_view_equal_ci(name, "Content-Encoding") &&
            !http_view_equal_ci(value, "identity"))
            return fail(d, TMS_DL_HTTP);
        if (http_view_equal_ci(name, "Content-Range"))
            return fail(d, TMS_DL_HTTP);
    }
    if (OOP_CALL(file(), exists, d->path) &&
        OOP_CALL(file(), remove, d->path) != 0)
        return fail(d, TMS_DL_FILE);
    d->owns_file = true; /* open failure can still leave a newly created file */
    d->handle    = OOP_CALL(file(), open, d->path, "wb");
    return d->handle ? 0 : fail(d, TMS_DL_FILE);
}
static int body(void* user, const uint8_t* bytes, size_t length) {
    TmsDownload* d = user;
    size_t       written;
    if (!d->handle)
        return fail(d, TMS_DL_FILE);
    if (length > d->status.expected - d->status.written)
        return fail(d, TMS_DL_SIZE);
    if (!length)
        return 0;
    written = OOP_CALL(file(), write, bytes, 1, length, d->handle);
    if (written > length)
        return fail(d, TMS_DL_FILE);
    d->status.written += (uint32_t)written;
    if (written != length)
        return fail(d, TMS_DL_FILE);
    if (d->progress) {
        d->in_callback = true;
        d->progress(d->status.written, d->status.expected, d->user);
        d->in_callback = false;
    }
    return 0;
}
static void completed(HttpClient* client, const HttpClientStatus* s,
                      void* user) {
    TmsDownload* d = user;
    long         size;
    (void)client;
    d->status.http = *s;
    if (d->status.result == TMS_DL_OK) {
        if (s->result == HTTP_CLIENT_CANCELLED)
            d->status.result = TMS_DL_CANCELLED;
        else if (s->result == HTTP_CLIENT_HTTP_ERROR)
            d->status.result = TMS_DL_HTTP;
        else if (s->result != HTTP_CLIENT_OK)
            d->status.result = TMS_DL_NETWORK;
        else if (s->http_status != 200)
            d->status.result = TMS_DL_HTTP;
        else if (d->status.written != d->status.expected)
            d->status.result = TMS_DL_SIZE;
        else if (!d->handle)
            d->status.result = TMS_DL_FILE;
    }
    if (d->handle) {
        if (d->status.result == TMS_DL_OK) {
            if (OOP_CALL(file(), flush, d->handle) != 0 ||
                OOP_CALL(file(), sync, d->handle) != 0)
                d->status.result = TMS_DL_FILE;
            else {
                size = OOP_CALL(file(), size, d->handle);
                if (size < 0 || (uint64_t)size != d->status.expected)
                    d->status.result = TMS_DL_SIZE;
            }
        }
        if (OOP_CALL(file(), close, d->handle) != 0 &&
            d->status.result == TMS_DL_OK)
            d->status.result = TMS_DL_FILE;
        d->handle = NULL;
    }
    if (d->status.result != TMS_DL_OK && d->owns_file &&
        OOP_CALL(file(), exists, d->path))
        d->status.cleanup_failed = OOP_CALL(file(), remove, d->path) != 0;
    d->owns_file = false;
    d->busy      = false;
    if (d->done) {
        d->in_callback = true;
        d->done(d, &d->status, d->user);
        d->in_callback = false;
    }
}
TmsDownloadResult tms_download_start(TmsDownload* d, const char* url,
                                     const char* path, uint32_t expected,
                                     uint32_t            timeout,
                                     TmsDownloadProgress progress,
                                     TmsDownloadDone done, void* user) {
    HttpClientRequest q = {0};
    HttpClientResult  result;
    TmsDownloadResult rc;
    HttpHeader        h = {HTTP_VIEW_LITERAL("Accept-Encoding"),
                           HTTP_VIEW_LITERAL("identity")};
    if (!d)
        return TMS_DL_INVALID;
    if (d->busy || d->in_callback || d->client.busy || d->client.in_callback)
        return TMS_DL_BUSY;
    if (!url || !path || !*path || strlen(path) >= sizeof(d->path) ||
        !expected || !timeout || timeout > INT32_MAX)
        return TMS_DL_INVALID;
#if LONG_MAX < UINT32_MAX
    if (expected > (uint32_t)LONG_MAX)
        return TMS_DL_INVALID;
#endif
    rc = parse_url(d, url);
    if (rc != TMS_DL_OK)
        return rc;
    memset(&d->status, 0, sizeof(d->status));
    d->status.expected = expected;
    strcpy(d->path, path);
    d->handle                       = NULL;
    d->owns_file                    = false;
    d->progress                     = progress;
    d->done                         = done;
    d->user                         = user;
    q.connect_host                  = d->host;
    q.port                          = d->port;
    q.timeout_ms                    = timeout;
    q.request.method                = HTTP_GET;
    q.request.host.ptr              = d->authority;
    q.request.host.len              = strlen(d->authority);
    q.request.target.ptr            = d->target;
    q.request.target.len            = strlen(d->target);
    q.request.headers               = &h;
    q.request.header_count          = 1;
    q.parser_options                = http_parser_options();
    q.parser_options.max_body_bytes = expected;
    q.parser_options.max_wire_bytes = (uint64_t)expected * 32 + 65536;
    q.callbacks.on_headers          = headers;
    q.callbacks.on_body             = body;
    q.on_done                       = completed;
    q.user                          = d;
    d->busy                         = true;
    result = http_client_start(&d->client, &q, d->headers, sizeof(d->headers));
    if (result != HTTP_CLIENT_OK) {
        d->busy               = false;
        d->status.http        = d->client.status;
        d->status.http.result = result;
        d->status.result =
            result == HTTP_CLIENT_HTTP_ERROR ? TMS_DL_HTTP : TMS_DL_NETWORK;
        return d->status.result;
    }
    return TMS_DL_OK;
}
TmsDownloadResult tms_download_cancel(TmsDownload* d) {
    if (!d)
        return TMS_DL_INVALID;
    if (d->in_callback || d->client.in_callback)
        return TMS_DL_BUSY;
    return http_client_cancel(&d->client) == HTTP_CLIENT_OK ? TMS_DL_OK
                                                            : TMS_DL_BUSY;
}
