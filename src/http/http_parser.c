#include "http_internal.h"
#include <limits.h>
#include "logger.h"
#include <string.h>

#if HTTP_MAX_HEADERS < 1 || HTTP_CHUNK_SCRATCH_SIZE < 1
#error HTTP buffer counts must be positive
#endif

static HttpResult fail(HttpParser* p, HttpResult error) {
    p->state = HTTP_PARSE_FAILED;
    p->error = error;
    return error;
}

HttpParserOptions http_parser_options(void) {
    HttpParserOptions o;
    o.response_to_head      = false;
    o.allow_close_delimited = true;
    o.max_body_bytes        = UINT64_C(16) * 1024 * 1024;
    o.max_wire_bytes        = UINT64_C(32) * 1024 * 1024;
    o.max_informational     = 8;
    return o;
}

HttpResult http_parser_init(HttpParser* p, char* buffer, size_t cap,
                            const HttpParserOptions* options, HttpCallbacks cb,
                            void* user) {
    HttpParserOptions o;
    if (!p) {
        LOG_TRACE("invalid parser memory");
        return HTTP_ERR_ARGUMENT;
    }
    o = options ? *options : http_parser_options();
    memset(p, 0, sizeof(*p));
    if (!buffer || cap < 4 || cap > INT_MAX) {
        LOG_TRACE("cap = %d", cap);
        return HTTP_ERR_ARGUMENT;
    }
    p->header_buffer   = buffer;
    p->header_capacity = cap;
    p->callbacks       = cb;
    p->user            = user;
    p->options         = o;
    p->state           = HTTP_PARSE_HEADERS;
    /* Stop at the end of the zero chunk; we validate trailers ourselves. */
    p->chunked.consume_trailer = 0;
    p->initialized             = true;
    return HTTP_OK;
}

/* Strict CRLF for status/header/trailer lines; reject obsolete folding later.
 */
static HttpResult collect(HttpParser* p, unsigned char c, bool trailers) {
    size_t n = p->header_length;
    if (n == p->header_capacity)
        return HTTP_ERR_CAPACITY;
    if ((c == '\n' && (n == 0 || p->header_buffer[n - 1] != '\r')) ||
        (n && p->header_buffer[n - 1] == '\r' && c != '\n') || c == 0)
        return HTTP_ERR_FORMAT;
    p->header_buffer[n++] = (char)c;
    p->header_length      = n;
    if (trailers && n == 2 && p->header_buffer[0] == '\r' && c == '\n')
        return HTTP_DONE;
    if (n >= 4 && memcmp(p->header_buffer + n - 4, "\r\n\r\n", 4) == 0)
        return HTTP_DONE;
    return HTTP_MORE;
}

static HttpResult connection_tokens(HttpView value, bool* close, bool* keep) {
    size_t start = 0, i;
    if (!value.len)
        return HTTP_ERR_FORMAT;
    for (i = 0; i <= value.len; ++i) {
        if (i == value.len || value.ptr[i] == ',') {
            HttpView t = {value.ptr + start, i - start};
            t          = http_trim(t);
            if (!http_token(t))
                return HTTP_ERR_FORMAT;
            if (http_view_equal_ci(t, "close"))
                *close = true;
            if (http_view_equal_ci(t, "keep-alive"))
                *keep = true;
            start = i + 1;
        }
    }
    return HTTP_OK;
}

static HttpResult validate_fields(HttpParser* p, size_t count, bool trailers,
                                  bool* chunked, bool* close, bool* keep) {
    size_t i;
    bool   seen_length = false, seen_transfer = false;
    for (i = 0; i < count; ++i) {
        HttpView   name = {p->headers[i].name, p->headers[i].name_len};
        HttpView   v    = {p->headers[i].value, p->headers[i].value_len};
        HttpResult rc;
        /* name==NULL means obsolete folded header. */
        if (!http_token(name) || !http_field_value(v))
            return HTTP_ERR_FORMAT;
        v = http_trim(v);
        if (trailers) {
            /* Trailer values cannot change routing, framing or representation.
             * Other well-formed trailers are ignored, never merged into
             * headers. */
            if (http_view_equal_ci(name, "Content-Length") ||
                http_view_equal_ci(name, "Transfer-Encoding") ||
                http_view_equal_ci(name, "Host") ||
                http_view_equal_ci(name, "Connection") ||
                http_view_equal_ci(name, "Trailer") ||
                http_view_equal_ci(name, "Content-Range") ||
                http_view_equal_ci(name, "Content-Encoding") ||
                http_view_equal_ci(name, "Content-Type"))
                return HTTP_ERR_FORMAT;
            continue;
        }
        if (http_view_equal_ci(name, "Content-Length")) {
            /* Deliberately reject ALL duplicates, including identical values.
             */
            if (seen_length)
                return HTTP_ERR_FORMAT;
            seen_length = true;
            rc          = http_decimal(v, &p->response.content_length);
            if (rc != HTTP_OK)
                return rc;
            p->response.has_content_length = true;
        } else if (http_view_equal_ci(name, "Transfer-Encoding")) {
            if (seen_transfer)
                return HTTP_ERR_FORMAT;
            seen_transfer = true;
            if (!http_view_equal_ci(v, "chunked"))
                return HTTP_ERR_UNSUPPORTED;
            *chunked = true;
        } else if (http_view_equal_ci(name, "Connection")) {
            rc = connection_tokens(v, close, keep);
            if (rc != HTTP_OK)
                return rc;
        }
    }
    if (seen_transfer && seen_length)
        return HTTP_ERR_FORMAT;
    return HTTP_OK;
}

static HttpResult parse_headers(HttpParser* p) {
    int           minor, status, parsed;
    const char*   message;
    size_t        message_len, count = HTTP_MAX_HEADERS;
    bool          chunked = false, close = false, keep = false, no_body;
    HttpResult    rc;
    HttpResponse* r = &p->response;
    memset(r, 0, sizeof(*r));
    parsed =
        phr_parse_response(p->header_buffer, p->header_length, &minor, &status,
                           &message, &message_len, p->headers, &count, 0);
    if (parsed < 0 || (size_t)parsed != p->header_length)
        return HTTP_ERR_FORMAT;
    if (minor != 0 && minor != 1)
        return HTTP_ERR_UNSUPPORTED;
    if (status < 100 || status > 599)
        return HTTP_ERR_FORMAT;
    {
        HttpView reason = {message, message_len};
        if (!http_field_value(reason))
            return HTTP_ERR_FORMAT;
    }
    if (status == 101)
        return HTTP_ERR_UNSUPPORTED;
    r->status        = (unsigned)status;
    r->minor_version = (unsigned)minor;
    rc = validate_fields(p, count, false, &chunked, &close, &keep);
    if (rc != HTTP_OK)
        return rc;
    if (minor == 0 && chunked)
        return HTTP_ERR_UNSUPPORTED;
    if ((status < 200 || status == 204) && (chunked || r->has_content_length))
        return HTTP_ERR_FORMAT;
    r->connection_close = close || (minor == 0 && !keep);
    no_body = p->options.response_to_head || status < 200 || status == 204 ||
              status == 304;
    if (no_body)
        r->body_mode = HTTP_BODY_NONE;
    else if (chunked)
        r->body_mode = HTTP_BODY_CHUNKED;
    else if (r->has_content_length)
        r->body_mode = HTTP_BODY_LENGTH;
    else {
        if (!p->options.allow_close_delimited)
            return HTTP_ERR_UNSUPPORTED;
        r->body_mode        = HTTP_BODY_UNTIL_CLOSE;
        r->connection_close = true;
    }
    if (!no_body && r->has_content_length &&
        (r->content_length > p->options.max_body_bytes ||
         (status == 205 && r->content_length != 0)))
        return status == 205 ? HTTP_ERR_FORMAT : HTTP_ERR_LIMIT;
    if (status < 200) {
        if (p->informational_count >= p->options.max_informational)
            return HTTP_ERR_LIMIT;
        ++p->informational_count;
    }
    if (p->callbacks.on_headers &&
        p->callbacks.on_headers(p->user, r, p->headers, count) != 0)
        return HTTP_ERR_CALLBACK;
    if (status < 200) {
        p->header_length = 0;
        p->state         = HTTP_PARSE_HEADERS;
        return HTTP_MORE;
    }
    switch (r->body_mode) {
    case HTTP_BODY_NONE:
        p->state = HTTP_PARSE_DONE;
        break;
    case HTTP_BODY_LENGTH:
        p->state = r->content_length ? HTTP_PARSE_LENGTH : HTTP_PARSE_DONE;
        break;
    case HTTP_BODY_CHUNKED:
        p->state = HTTP_PARSE_CHUNKED;
        break;
    case HTTP_BODY_UNTIL_CLOSE:
        p->state = HTTP_PARSE_CLOSE;
        break;
    }
    return p->state == HTTP_PARSE_DONE ? HTTP_DONE : HTTP_MORE;
}

static HttpResult parse_trailers(HttpParser* p) {
    size_t     count = HTTP_MAX_HEADERS;
    bool       a = false, b = false, c = false;
    int        parsed = phr_parse_headers(p->header_buffer, p->header_length,
                                          p->headers, &count, 0);
    HttpResult rc;
    if (parsed < 0 || (size_t)parsed != p->header_length)
        return HTTP_ERR_FORMAT;
    rc = validate_fields(p, count, true, &a, &b, &c);
    if (rc != HTTP_OK)
        return rc;
    p->state = HTTP_PARSE_DONE;
    return HTTP_DONE;
}

static HttpResult body(HttpParser* p, const uint8_t* data, size_t size) {
    if (!size)
        return HTTP_OK;
    if (p->response.status == 205)
        return HTTP_ERR_FORMAT;
    if ((uint64_t)size > p->options.max_body_bytes - p->response.body_received)
        return HTTP_ERR_LIMIT;
    if (p->callbacks.on_body && p->callbacks.on_body(p->user, data, size) != 0)
        return HTTP_ERR_CALLBACK;
    p->response.body_received += (uint64_t)size;
    return HTTP_OK;
}

HttpResult http_parser_feed(HttpParser* p, const uint8_t* input, size_t len,
                            size_t* consumed) {
    size_t     pos = 0;
    HttpResult rc  = HTTP_MORE;
    if (!consumed)
        return HTTP_ERR_ARGUMENT;
    *consumed = 0;
    if (!p || !p->initialized || (!input && len))
        return HTTP_ERR_ARGUMENT;
    if (p->state == HTTP_PARSE_FAILED)
        return p->error;
    if (p->state == HTTP_PARSE_DONE)
        return HTTP_DONE;
    while (pos < len && p->state != HTTP_PARSE_DONE) {
        uint64_t budget = p->options.max_wire_bytes - p->wire_received;
        size_t   n;
        if (!budget) {
            rc = HTTP_ERR_LIMIT;
            break;
        }
        if (p->state == HTTP_PARSE_HEADERS || p->state == HTTP_PARSE_TRAILERS) {
            bool trailers = p->state == HTTP_PARSE_TRAILERS;
            rc            = collect(p, input[pos++], trailers);
            ++p->wire_received;
            if (rc < 0)
                break;
            if (rc == HTTP_DONE) {
                rc = trailers ? parse_trailers(p) : parse_headers(p);
                if (rc < 0)
                    break;
            }
            continue;
        }
        n = len - pos;
        if ((uint64_t)n > budget)
            n = (size_t)budget;
        if (p->state == HTTP_PARSE_LENGTH) {
            uint64_t remaining =
                p->response.content_length - p->response.body_received;
            if ((uint64_t)n > remaining)
                n = (size_t)remaining;
            rc = body(p, input + pos, n);
            pos += n;
            p->wire_received += (uint64_t)n;
            if (rc < 0)
                break;
            if (p->response.body_received == p->response.content_length)
                p->state = HTTP_PARSE_DONE;
        } else if (p->state == HTTP_PARSE_CLOSE) {
            rc = body(p, input + pos, n);
            pos += n;
            p->wire_received += (uint64_t)n;
            if (rc < 0)
                break;
        } else if (p->state == HTTP_PARSE_CHUNKED) {
            size_t   decoded, used;
            intptr_t result;
            if (n > sizeof(p->chunk_scratch))
                n = sizeof(p->chunk_scratch);
            memcpy(p->chunk_scratch, input + pos, n);
            decoded = n;
            result =
                phr_decode_chunked(&p->chunked, p->chunk_scratch, &decoded);
            used = result >= 0 ? n - (size_t)result : n;
            pos += used;
            p->wire_received += (uint64_t)used;
            if (result == -1) {
                rc = HTTP_ERR_FORMAT;
                break;
            }
            rc = body(p, (const uint8_t*)p->chunk_scratch, decoded);
            if (rc < 0)
                break;
            if (result >= 0) {
                p->header_length = 0;
                p->state         = HTTP_PARSE_TRAILERS;
            }
        } else {
            rc = HTTP_ERR_STATE;
            break;
        }
    }
    *consumed = pos;
    if (rc < 0)
        return fail(p, rc);
    return p->state == HTTP_PARSE_DONE ? HTTP_DONE : HTTP_MORE;
}

HttpResult http_parser_eof(HttpParser* p) {
    if (!p || !p->initialized)
        return HTTP_ERR_ARGUMENT;
    if (p->state == HTTP_PARSE_FAILED)
        return p->error;
    if (p->state == HTTP_PARSE_DONE)
        return HTTP_DONE;
    if (p->state == HTTP_PARSE_CLOSE) {
        p->state = HTTP_PARSE_DONE;
        return HTTP_DONE;
    }
    return fail(p, HTTP_ERR_TRUNCATED);
}
