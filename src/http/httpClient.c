#include "httpClient.h"
#include <string.h>

static void finish(HttpClient* c, HttpClientResult result,
                   NthResult nth_result) {
    NthTransaction* tx;
    if (!c || !c->busy)
        return;
    c->status.result      = result;
    c->status.nth_result  = nth_result;
    c->status.http_status = c->parser.response.status;
    c->status.body_bytes  = c->parser.response.body_received;
    tx                    = c->tx;
    c->tx                 = NULL;
    c->busy               = false;
    if (tx) {
        /* Detach first; release owns closing and returning the transaction. */
        tx->onConnect  = NULL;
        tx->onSent     = NULL;
        tx->onChunk    = NULL;
        tx->isComplete = NULL;
        tx->onReceive  = NULL;
        tx->onFailure  = NULL;
        tx->onTimeout  = NULL;
        tx->userData   = NULL;
        nth()->release(tx);
    }
    if (!c->starting && c->on_done) {
        c->in_callback = true;
        c->on_done(c, &c->status, c->user);
        c->in_callback = false;
    }
}

static int on_headers(void* user, const HttpResponse* response,
                      const struct phr_header* headers, size_t count) {
    HttpClient* c      = user;
    int         result = 0;
    if (c->callbacks.on_headers) {
        c->in_callback = true;
        result = c->callbacks.on_headers(c->user, response, headers, count);
        c->in_callback = false;
    }
    return result;
}

static int on_body(void* user, const uint8_t* bytes, size_t length) {
    HttpClient* c      = user;
    int         result = 0;
    if (c->callbacks.on_body) {
        c->in_callback = true;
        result         = c->callbacks.on_body(c->user, bytes, length);
        c->in_callback = false;
    }
    return result;
}

static int8_t on_connect(NthTransaction* tx, void* user) {
    HttpClient* c = user;
    NthResult   result;
    if (!c || !c->busy || c->tx != tx)
        return 0;
    /* Already built directly into txBuffer before connecting. Do not call
     * nth.send(tx, &tx->txBuffer): it uses memcpy on overlapping storage. */
    result = nth()->sendProvidedTx(tx);
    if (result != NTH_OK && c->busy)
        finish(c, HTTP_CLIENT_NETWORK_ERROR, result);
    return 0;
}

static void on_chunk(NthTransaction* tx, ByteArray* chunk, void* user) {
    HttpClient* c        = user;
    size_t      consumed = 0;
    if (!c || !c->busy || c->tx != tx)
        return;
    if (!chunk || !chunk->data || chunk->len > chunk->capacity) {
        c->status.http_result = HTTP_ERR_ARGUMENT;
        return;
    }
    if (c->status.http_result < 0)
        return;
    c->status.http_result =
        http_parser_feed(&c->parser, chunk->data, chunk->len, &consumed);
    /* One response per connection. Unexpected bytes after DONE are an error. */
    if (c->status.http_result >= 0 && consumed != chunk->len)
        c->status.http_result = HTTP_ERR_FORMAT;
    /* Leave NTH state alone. isComplete maps this result and NTH performs
     * cleanup before invoking its terminal callback. */
}

static NthRxDecision is_complete(NthTransaction* tx, void* user) {
    HttpClient* c = user;
    if (!c || !c->busy || c->tx != tx)
        return NTH_RX_ERROR;
    if (c->status.http_result < 0)
        return NTH_RX_ERROR;
    if (tx->peerClosed)
        c->status.http_result = http_parser_eof(&c->parser);
    if (c->status.http_result == HTTP_DONE)
        return NTH_RX_COMPLETE;
    return c->status.http_result < 0 ? NTH_RX_ERROR : NTH_RX_MORE;
}

static int8_t on_receive(NthTransaction* tx, void* user) {
    HttpClient* c = user;
    if (c && c->busy && c->tx == tx) {
        if (c->status.http_result != HTTP_DONE) {
            c->status.http_result = HTTP_ERR_STATE;
            finish(c, HTTP_CLIENT_HTTP_ERROR, NTH_OK);
        } else
            finish(c, HTTP_CLIENT_OK, NTH_OK);
    }
    return 0;
}

static int8_t on_failure(NthTransaction* tx, void* user) {
    HttpClient* c = user;
    if (c && c->busy && c->tx == tx)
        finish(c,
               c->status.http_result < 0 ? HTTP_CLIENT_HTTP_ERROR
                                         : HTTP_CLIENT_NETWORK_ERROR,
               tx->lastError);
    return 0;
}

static int8_t on_timeout(NthTransaction* tx, void* user) {
    HttpClient* c = user;
    if (c && c->busy && c->tx == tx)
        finish(c, HTTP_CLIENT_TIMEOUT, tx->lastError);
    /* A timeout must never be passed to http_parser_eof(). */
    return 0;
}

HttpClientResult http_client_start(HttpClient* c, const HttpClientRequest* q,
                                   char*  header_storage,
                                   size_t header_capacity) {
    HttpCallbacks     callbacks = {on_headers, on_body};
    HttpParserOptions options;
    HttpRequest       request;
    HttpResult        hr;
    NthResult         nr;
    size_t            header_length;
    if (!c)
        return HTTP_CLIENT_INVALID;
    if (c->busy || c->in_callback || c->starting)
        return HTTP_CLIENT_BUSY;
    if (!q || !q->connect_host || !q->connect_host[0] || !q->port ||
        !q->timeout_ms || q->timeout_ms > INT32_MAX ||
        (!q->body && q->body_length) ||
        q->request.body_length != (uint64_t)q->body_length)
        return HTTP_CLIENT_INVALID;
    memset(c, 0, sizeof(*c));
    c->status.http_result    = HTTP_MORE;
    c->status.nth_result     = NTH_OK;
    c->callbacks             = q->callbacks;
    c->on_done               = q->on_done;
    c->user                  = q->user;
    options                  = q->parser_options;
    options.response_to_head = q->request.method == HTTP_HEAD;
    hr = http_parser_init(&c->parser, header_storage, header_capacity, &options,
                          callbacks, c);
    if (hr != HTTP_OK) {
        c->status.result      = HTTP_CLIENT_HTTP_ERROR;
        c->status.http_result = hr;
        return c->status.result;
    }
    c->tx = nth()->alloc();
    if (!c->tx) {
        c->status.result = HTTP_CLIENT_NO_TRANSACTION;
        return c->status.result;
    }
    c->busy            = true;
    c->starting        = true;
    request            = q->request;
    request.keep_alive = false;
    hr = http_build_request(&request, (char*)c->tx->txBuffer.data,
                            c->tx->txBuffer.capacity, &header_length);
    if (hr == HTTP_OK &&
        q->body_length > c->tx->txBuffer.capacity - header_length)
        hr = HTTP_ERR_CAPACITY;
    if (hr != HTTP_OK) {
        c->status.http_result = hr;
        finish(c, HTTP_CLIENT_HTTP_ERROR, NTH_OK);
        c->starting = false;
        return c->status.result;
    }
    if (q->body_length)
        memcpy(c->tx->txBuffer.data + header_length, q->body, q->body_length);
    c->tx->txBuffer.len = header_length + q->body_length;
    c->tx->streamRx     = true;
    c->tx->timeoutMs    = q->timeout_ms;
    c->tx->userData     = c;
    c->tx->onConnect    = on_connect;
    c->tx->onChunk      = on_chunk;
    c->tx->isComplete   = is_complete;
    c->tx->onReceive    = on_receive;
    c->tx->onFailure    = on_failure;
    c->tx->onTimeout    = on_timeout;
    /* Keep onSent NULL; NTH resets txBuffer.len after that callback. */
    nr = nth()->connect(c->tx, q->connect_host, q->port);
    if (nr != NTH_OK && c->busy)
        finish(c, HTTP_CLIENT_NETWORK_ERROR, nr);
    c->starting = false;
    return c->busy ? HTTP_CLIENT_OK : c->status.result;
}

HttpClientResult http_client_cancel(HttpClient* c) {
    if (!c)
        return HTTP_CLIENT_INVALID;
    if (c->in_callback || c->starting)
        return HTTP_CLIENT_BUSY;
    if (c->busy)
        finish(c, HTTP_CLIENT_CANCELLED, NTH_OK);
    return HTTP_CLIENT_OK;
}
