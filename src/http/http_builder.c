#include "http_internal.h"
#include <string.h>

typedef struct { char *data; size_t capacity, length; } Writer;

static bool append(Writer *w, const char *data, size_t length)
{
    if (length > w->capacity - w->length) return false;
    if (length) memcpy(w->data + w->length, data, length);
    w->length += length;
    return true;
}

static bool literal(Writer *w, const char *s) { return append(w, s, strlen(s)); }

static bool number(Writer *w, uint64_t n)
{
    char tmp[20];
    size_t pos = sizeof(tmp);
    do { tmp[--pos] = (char)('0' + n % 10); n /= 10; } while (n);
    return append(w, tmp + pos, sizeof(tmp) - pos);
}

static bool managed(HttpView name)
{
    return http_view_equal_ci(name, "Host") || http_view_equal_ci(name, "Connection") ||
        http_view_equal_ci(name, "Content-Length") || http_view_equal_ci(name, "Transfer-Encoding") ||
        http_view_equal_ci(name, "Expect") || http_view_equal_ci(name, "Upgrade") ||
        http_view_equal_ci(name, "Trailer");
}

static bool equal_names(HttpView a, HttpView b)
{
    size_t i;
    if (a.len != b.len) return false;
    for (i = 0; i < a.len; ++i) {
        unsigned char x = (unsigned char)a.ptr[i], y = (unsigned char)b.ptr[i];
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
        if (x != y) return false;
    }
    return true;
}

static bool target_valid(HttpView v, HttpMethod method)
{
    size_t i;
    if (!v.ptr || !v.len) return false;
    if (v.ptr[0] != '/' && !(method == HTTP_OPTIONS && v.len == 1 && v.ptr[0] == '*')) return false;
    for (i = 0; i < v.len; ++i) {
        unsigned char c = (unsigned char)v.ptr[i];
        if (c <= 32 || c >= 127 || c == '#' || c == '\\') return false;
    }
    return true;
}

/* Authority validation prevents routing/injection delimiters. DNS and IPv6
 * address validity are responsibilities of the URL/transport layer. */
static bool host_valid(HttpView v)
{
    size_t i, end, colon = v.len;
    HttpView port;
    uint64_t n;
    if (!v.ptr || !v.len) return false;
    if (v.ptr[0] == '[') {
        for (end = 1; end < v.len && v.ptr[end] != ']'; ++end) {
            unsigned char c = (unsigned char)v.ptr[end];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F') || c == ':' || c == '.')) return false;
        }
        if (end == v.len || end == 1) return false;
        if (end + 1 < v.len) {
            if (v.ptr[end + 1] != ':') return false;
            colon = end + 1;
        }
    } else {
        for (i = 0; i < v.len; ++i) {
            unsigned char c = (unsigned char)v.ptr[i];
            if (c == ':') { colon = i; break; }
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                  (c >= 'A' && c <= 'Z') || c == '-' || c == '.')) return false;
        }
        if (!colon) return false;
    }
    if (colon < v.len) {
        port.ptr = v.ptr + colon + 1; port.len = v.len - colon - 1;
        if (http_decimal(port, &n) != HTTP_OK || n == 0 || n > 65535) return false;
    }
    return true;
}

HttpResult http_build_request(const HttpRequest *r, char *out, size_t cap, size_t *written)
{
    static const char *const methods[] = { "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS" };
    Writer w = { out, cap, 0 };
    size_t i, j;
    if (!written) return HTTP_ERR_ARGUMENT;
    *written = 0;
    if (!r || !out || (r->header_count && !r->headers)) return HTTP_ERR_ARGUMENT;
    if ((unsigned)r->method >= sizeof(methods) / sizeof(methods[0])) return HTTP_ERR_ARGUMENT;
    if (r->header_count > HTTP_MAX_HEADERS) return HTTP_ERR_LIMIT;
    if (!target_valid(r->target, r->method) || !host_valid(r->host)) return HTTP_ERR_FORMAT;
    for (i = 0; i < r->header_count; ++i) {
        if (!http_token(r->headers[i].name) || !http_field_value(r->headers[i].value)) return HTTP_ERR_FORMAT;
        if (managed(r->headers[i].name)) return HTTP_ERR_ARGUMENT;
        for (j = 0; j < i; ++j)
            if (equal_names(r->headers[i].name, r->headers[j].name)) return HTTP_ERR_ARGUMENT;
    }
    if (!literal(&w, methods[r->method]) || !literal(&w, " ") ||
        !append(&w, r->target.ptr, r->target.len) || !literal(&w, " HTTP/1.1\r\nHost: ") ||
        !append(&w, r->host.ptr, r->host.len) ||
        !literal(&w, r->keep_alive ? "\r\nConnection: keep-alive\r\n" : "\r\nConnection: close\r\n"))
        return HTTP_ERR_CAPACITY;
    for (i = 0; i < r->header_count; ++i) {
        if (!append(&w, r->headers[i].name.ptr, r->headers[i].name.len) || !literal(&w, ": ") ||
            !append(&w, r->headers[i].value.ptr, r->headers[i].value.len) || !literal(&w, "\r\n"))
            return HTTP_ERR_CAPACITY;
    }
    if (r->body_length || r->method == HTTP_POST || r->method == HTTP_PUT || r->method == HTTP_PATCH) {
        if (!literal(&w, "Content-Length: ") || !number(&w, r->body_length) || !literal(&w, "\r\n"))
            return HTTP_ERR_CAPACITY;
    }
    if (!literal(&w, "\r\n")) return HTTP_ERR_CAPACITY;
    *written = w.length;
    return HTTP_OK;
}

HttpResult http_build_range(uint64_t start, uint64_t count, char *out, size_t cap, size_t *written)
{
    Writer w = { out, cap, 0 };
    if (!written) return HTTP_ERR_ARGUMENT;
    *written = 0;
    if (!out || !count) return HTTP_ERR_ARGUMENT;
    if (count - 1 > UINT64_MAX - start) return HTTP_ERR_OVERFLOW;
    if (!literal(&w, "bytes=") || !number(&w, start) || !literal(&w, "-") ||
        !number(&w, start + count - 1)) return HTTP_ERR_CAPACITY;
    if (w.length == cap) return HTTP_ERR_CAPACITY;
    out[w.length] = '\0';
    *written = w.length;
    return HTTP_OK;
}
