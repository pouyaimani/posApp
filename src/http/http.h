#ifndef HTTP_LAYER_H
#define HTTP_LAYER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "http/picohttpparser/picohttpparser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Define consistently in every translation unit. No heap allocation is used. */
#ifndef HTTP_MAX_HEADERS
#define HTTP_MAX_HEADERS 24
#endif
#ifndef HTTP_CHUNK_SCRATCH_SIZE
#define HTTP_CHUNK_SCRATCH_SIZE 256
#endif

typedef enum {
    HTTP_OK              = 0,
    HTTP_MORE            = 1,
    HTTP_DONE            = 2,
    HTTP_ERR_ARGUMENT    = -1,
    HTTP_ERR_STATE       = -2,
    HTTP_ERR_CAPACITY    = -3,
    HTTP_ERR_FORMAT      = -4,
    HTTP_ERR_OVERFLOW    = -5,
    HTTP_ERR_UNSUPPORTED = -6,
    HTTP_ERR_LIMIT       = -7,
    HTTP_ERR_TRUNCATED   = -8,
    HTTP_ERR_CALLBACK    = -9
} HttpResult;

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_HEAD,
    HTTP_OPTIONS
} HttpMethod;

/* String views are not NUL-terminated. Input views must point to readable
 * bytes. */
typedef struct {
    const char* ptr;
    size_t      len;
} HttpView;
#define HTTP_VIEW_LITERAL(s) {(s), sizeof(s) - 1u}
typedef struct {
    HttpView name, value;
} HttpHeader;

typedef struct {
    HttpMethod        method;
    HttpView          target;  /* Origin form: /path?query; already encoded. */
    HttpView          host;    /* Host[:port], IPv6 literal uses brackets. */
    const HttpHeader* headers; /* Optional additional headers. */
    size_t            header_count;
    uint64_t body_length; /* Exact body bytes the transport must send. */
    bool     keep_alive;  /* false emits Connection: close. */
} HttpRequest;

/* Builds headers ONLY, including final CRLF. Body remains caller-owned.
 * Emits Content-Length for POST/PUT/PATCH, or whenever body_length != 0.
 * Host, Connection, Content-Length, Transfer-Encoding, Expect, Upgrade and
 * Trailer are managed/unsupported and cannot be added as custom headers.
 * Duplicate custom names are rejected. On error *written=0; discard output.
 * Input must not overlap output. No terminating NUL is required or emitted. */
HttpResult http_build_request(const HttpRequest* request, char* output,
                              size_t capacity, size_t* written);

/* Format an inclusive byte range from start + count (count must be >0).
 * Result is NUL-terminated; *written excludes NUL. */
HttpResult http_build_range(uint64_t start, uint64_t count, char* output,
                            size_t capacity, size_t* written);

typedef struct {
    bool     unsatisfied; /* Asterisk instead of first-last. */
    bool     total_known; /* false when the total is an asterisk. */
    uint64_t first, last, total;
} HttpContentRange;

/* Parses satisfied and unsatisfied byte ranges, including unknown totals.
 * Rejects inconsistent bounds. */
HttpResult  http_parse_content_range(HttpView value, HttpContentRange* range);
bool        http_view_equal_ci(HttpView value, const char* literal);
const char* http_result_string(HttpResult result);

typedef enum {
    HTTP_BODY_NONE,
    HTTP_BODY_LENGTH,
    HTTP_BODY_CHUNKED,
    HTTP_BODY_UNTIL_CLOSE
} HttpBodyMode;

typedef struct {
    unsigned     status;
    unsigned     minor_version; /* HTTP/1.0 or HTTP/1.1 only. */
    HttpBodyMode body_mode;
    bool         has_content_length;
    uint64_t     content_length; /* On HEAD/304 this is metadata, not a body. */
    bool     connection_close; /* Includes HTTP/1.0 default and EOF framing. */
    uint64_t body_received;    /* Accepted decoded bytes. */
} HttpResponse;

/* Header views are valid ONLY during on_headers. Copy values you need later.
 * on_headers is called for informational responses too (except 101: rejected).
 * Return 0 to accept; nonzero rejects the response before body delivery.
 * on_body must consume/copy/write all bytes synchronously and return 0.
 * Returning nonzero is fatal, not backpressure. Callback memory is temporary.
 * Do not reenter, reset, or modify the parser from its callbacks.
 * Streamed data is provisional until HTTP_DONE and application integrity
 * checks. */
typedef int (*HttpHeadersFn)(void* user, const HttpResponse* response,
                             const struct phr_header* headers, size_t count);
typedef int (*HttpBodyFn)(void* user, const uint8_t* bytes, size_t length);
typedef struct {
    HttpHeadersFn on_headers;
    HttpBodyFn    on_body;
} HttpCallbacks;

typedef struct {
    bool     response_to_head;
    bool     allow_close_delimited;
    uint64_t max_body_bytes; /* 0 permits no body; UINT64_MAX disables. */
    uint64_t max_wire_bytes; /* Includes headers/interim/trailers/framing. */
    unsigned max_informational;
} HttpParserOptions;

typedef enum {
    HTTP_PARSE_HEADERS,
    HTTP_PARSE_LENGTH,
    HTTP_PARSE_CHUNKED,
    HTTP_PARSE_TRAILERS,
    HTTP_PARSE_CLOSE,
    HTTP_PARSE_DONE,
    HTTP_PARSE_FAILED
} HttpParseState;

/* Public for static allocation; fields are read-only to callers after init.
 * One context per in-flight response. No sockets, timers, RTOS or globals.
 * The caller's header buffer is reused for trailers and informational headers.
 */
typedef struct {
    HttpResponse               response;
    HttpParserOptions          options;
    HttpCallbacks              callbacks;
    void*                      user;
    char*                      header_buffer;
    size_t                     header_capacity, header_length;
    struct phr_header          headers[HTTP_MAX_HEADERS];
    struct phr_chunked_decoder chunked;
    char                       chunk_scratch[HTTP_CHUNK_SCRATCH_SIZE];
    uint64_t                   wire_received;
    unsigned                   informational_count;
    HttpParseState             state;
    HttpResult                 error;
    bool                       initialized;
} HttpParser;

/* Defaults: 16 MiB decoded, 32 MiB wire, 8 interim responses; EOF bodies
 * allowed. */
HttpParserOptions http_parser_options(void);
HttpResult        http_parser_init(HttpParser* parser, char* header_buffer,
                                   size_t                   header_capacity,
                                   const HttpParserOptions* options,
                                   HttpCallbacks callbacks, void* user);

/* Input is not modified. *consumed is exact, including on HTTP_DONE.
 * Bytes after *consumed belong to a subsequent message and are not consumed.
 * Do not overlap input with the parser or its header buffer. Empty input is OK.
 * Errors latch until reinitialization. No callback is repeated after DONE.
 * A null body callback deliberately discards body data. */
HttpResult http_parser_feed(HttpParser* parser, const uint8_t* input,
                            size_t length, size_t* consumed);

/* Call ONLY on an orderly peer EOF. Timeout/cancellation/socket errors must
 * abort externally; they cannot successfully terminate an EOF-delimited body.
 */
HttpResult http_parser_eof(HttpParser* parser);

/* Find first header, case-insensitive. Valid only in the header callback. */
bool http_find_header(const struct phr_header* headers, size_t count,
                      const char* name, HttpView* value);

#ifdef __cplusplus
}
#endif
#endif
