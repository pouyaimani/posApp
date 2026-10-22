#include "http_internal.h"
#include <string.h>

static unsigned char lower(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + ('a' - 'A')) : c;
}

bool http_view_equal_ci(HttpView v, const char *s)
{
    size_t i;
    if (!s || (!v.ptr && v.len)) return false;
    for (i = 0; i < v.len; ++i) {
        if (!s[i] || lower((unsigned char)v.ptr[i]) != lower((unsigned char)s[i]))
            return false;
    }
    return s[i] == '\0';
}

bool http_token(HttpView v)
{
    size_t i;
    if (!v.ptr || !v.len) return false;
    for (i = 0; i < v.len; ++i) {
        unsigned char c = (unsigned char)v.ptr[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9')) continue;
        if (!c || !strchr("!#$%&'*+-.^_`|~", c)) return false;
    }
    return true;
}

bool http_field_value(HttpView v)
{
    size_t i;
    if (!v.ptr && v.len) return false;
    for (i = 0; i < v.len; ++i) {
        unsigned char c = (unsigned char)v.ptr[i];
        if ((c < 32 && c != '\t') || c == 127) return false;
    }
    return true;
}

HttpView http_trim(HttpView v)
{
    while (v.len && (v.ptr[0] == ' ' || v.ptr[0] == '\t')) { ++v.ptr; --v.len; }
    while (v.len && (v.ptr[v.len - 1] == ' ' || v.ptr[v.len - 1] == '\t')) --v.len;
    return v;
}

HttpResult http_decimal(HttpView v, uint64_t *value)
{
    size_t i;
    uint64_t n = 0;
    if (!v.ptr || !v.len || !value) return HTTP_ERR_FORMAT;
    for (i = 0; i < v.len; ++i) {
        unsigned char c = (unsigned char)v.ptr[i];
        uint64_t digit;
        if (c < '0' || c > '9') return HTTP_ERR_FORMAT;
        digit = (uint64_t)(c - '0');
        if (n > (UINT64_MAX - digit) / 10) return HTTP_ERR_OVERFLOW;
        n = n * 10 + digit;
    }
    *value = n;
    return HTTP_OK;
}

bool http_find_header(const struct phr_header *h, size_t count,
                      const char *name, HttpView *value)
{
    size_t i;
    if (!h || !name || !value) return false;
    for (i = 0; i < count; ++i) {
        HttpView key = { h[i].name, h[i].name_len };
        if (h[i].name && http_view_equal_ci(key, name)) {
            value->ptr = h[i].value; value->len = h[i].value_len;
            *value = http_trim(*value);
            return true;
        }
    }
    value->ptr = NULL; value->len = 0;
    return false;
}

HttpResult http_parse_content_range(HttpView v, HttpContentRange *out)
{
    HttpContentRange r = { false, false, 0, 0, 0 };
    HttpView unit, a, b, total;
    size_t slash, dash;
    HttpResult rc;
    if (!out || (!v.ptr && v.len)) return HTTP_ERR_ARGUMENT;
    memset(out, 0, sizeof(*out));
    v = http_trim(v);
    if (v.len < 8) return HTTP_ERR_FORMAT;
    unit.ptr = v.ptr; unit.len = 5;
    if (!http_view_equal_ci(unit, "bytes") || v.ptr[5] != ' ') return HTTP_ERR_FORMAT;
    v.ptr += 6; v.len -= 6;
    for (slash = 0; slash < v.len && v.ptr[slash] != '/'; ++slash) {}
    if (slash == v.len) return HTTP_ERR_FORMAT;
    total.ptr = v.ptr + slash + 1; total.len = v.len - slash - 1;
    if (total.len == 1 && total.ptr[0] == '*') r.total_known = false;
    else {
        rc = http_decimal(total, &r.total);
        if (rc != HTTP_OK) return rc;
        r.total_known = true;
    }
    if (slash == 1 && v.ptr[0] == '*') {
        if (!r.total_known) return HTTP_ERR_FORMAT;
        r.unsatisfied = true;
    } else {
        for (dash = 0; dash < slash && v.ptr[dash] != '-'; ++dash) {}
        if (dash == slash) return HTTP_ERR_FORMAT;
        a.ptr = v.ptr; a.len = dash;
        b.ptr = v.ptr + dash + 1; b.len = slash - dash - 1;
        rc = http_decimal(a, &r.first);
        if (rc != HTTP_OK) return rc;
        rc = http_decimal(b, &r.last);
        if (rc != HTTP_OK) return rc;
        if (r.first > r.last || (r.total_known && r.last >= r.total)) return HTTP_ERR_FORMAT;
    }
    *out = r;
    return HTTP_OK;
}

const char *http_result_string(HttpResult r)
{
    switch (r) {
    case HTTP_OK: return "ok";
    case HTTP_MORE: return "need more data";
    case HTTP_DONE: return "message complete";
    case HTTP_ERR_ARGUMENT: return "invalid argument";
    case HTTP_ERR_STATE: return "invalid state";
    case HTTP_ERR_CAPACITY: return "buffer/header capacity exceeded";
    case HTTP_ERR_FORMAT: return "malformed HTTP";
    case HTTP_ERR_OVERFLOW: return "integer overflow";
    case HTTP_ERR_UNSUPPORTED: return "unsupported HTTP feature";
    case HTTP_ERR_LIMIT: return "configured limit exceeded";
    case HTTP_ERR_TRUNCATED: return "truncated response";
    case HTTP_ERR_CALLBACK: return "application callback rejected data";
    default: return "unknown HTTP result";
    }
}
