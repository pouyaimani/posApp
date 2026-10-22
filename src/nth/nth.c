#include "nth.h"
#include <string.h>
#include "error.h"

/*********************************************************************************************
 *                                                                                           *
 *                                      Warning
 *                  using heap for io storage does not work for t3rtos
 *                                                                                           *
 ********************************************************************************************/

static Nth __nth;

extern NthTransport sysTransport;

static NthTransaction g_transactions[NT_MAX_TRANSACTIONS];

static NthTransport* g_transport;

static void nth_fail(NthTransaction* tx, NthResult error);

#ifndef NTH_ENABLE_EVENT_BRIDGE
#define NTH_ENABLE_EVENT_BRIDGE 0
#endif

static bool isValidHostname(const char* host) {
    RETURN_VALUE_IF_NULL(host, ;, false);

    if (*host == '\0')
        return false;

    int labelLen = 0;

    while (*host) {
        char c = *host;

        if (c == '.') {
            /* Empty label is invalid. */
            if (labelLen == 0)
                return false;

            /* Label cannot end with '-'. */
            if (*(host - 1) == '-')
                return false;

            labelLen = 0;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '-') {

            /* Label cannot start with '-'. */
            if (labelLen == 0 && c == '-')
                return false;

            labelLen++;

            if (labelLen > 63)
                return false;
        } else {
            return false;
        }

        host++;
    }

    /* Must not end with '.' or '-'. */
    if (labelLen == 0)
        return false;

    if (*(host - 1) == '-')
        return false;

    return true;
}

static bool isValidIPv4(const char* ip) {
    RETURN_VALUE_IF_NULL(ip, ;, false);

    int num    = 0;
    int dots   = 0;
    int digits = 0;

    while (*ip) {
        if (*ip == '.') {
            // Empty section or too many dots
            if (digits == 0)
                return false;

            // Reset for next octet
            dots++;
            digits = 0;
            num    = 0;

            if (dots > 3)
                return false;
        } else if (isDigit((unsigned char)*ip)) {
            num = num * 10 + (*ip - '0');
            digits++;

            // Maximum value for IPv4 octet
            if (num > 255)
                return false;

            // Prevent numbers like 0000 or very long octets
            if (digits > 3)
                return false;
        } else {
            return false;
        }

        ip++;
    }

    // Must end with valid octet and exactly 3 dots
    return (dots == 3 && digits > 0);
}

static bool isValidHost(const char* host) {
    RETURN_VALUE_IF_NULL(host, ;, false);

    return isValidIPv4(host) || isValidHostname(host);
}

static void nth_resetRx(NthTransaction* tx) {
    tx->rxOffset     = 0U;
    tx->rxBuffer.len = 0U;
    tx->peerClosed   = false;
}

static void nth_resetTx(NthTransaction* tx) {
    tx->txOffset     = 0;
    tx->txBuffer.len = 0;
}

static uint32_t nth_getTick(void) { return NTH_GET_TICK(); }

void nth_init() {
    g_transport = &sysTransport;
    memset(g_transactions, 0, sizeof(g_transactions));
}

NthTransaction* nth_allocTransaction(void) {
    NTH_LOG("nth: allocating transaction ...");
    for (size_t i = 0; i < NT_MAX_TRANSACTIONS; i++) {

        NthTransaction* tx = &g_transactions[i];

        if (!tx->active) {

            memset(tx, 0, sizeof(*tx));
            tx->socketFd = NTH_SOCKET_CLOSE;
            tx->active   = true;

            tx->state = NTH_TX_IDLE;

            tx->txBuffer.data     = tx->ioStorage;
            tx->txBuffer.capacity = sizeof(tx->ioStorage);

            tx->rxBuffer.data     = tx->ioStorage;
            tx->rxBuffer.capacity = sizeof(tx->ioStorage);

            tx->timeoutMs = NT_DEFAULT_TIMEOUT_MS;
            NTH_LOG("nth: allocating transaction succeed.");
            return tx;
        }
    }
    NTH_LOG("nth: allocating transaction failed.");
    return NULL;
}

void nth_disconnect(NthTransaction* tx) {
    RETURN_IF_NULL(tx, ;);

    tx->connectRetryPending = false;

    if (tx->socketFd >= 0) {

        NTH_LOG("NTH: closing socket = %d", tx->socketFd);

        g_transport->close(tx->socketFd);
    }

    tx->socketFd = NTH_SOCKET_CLOSE;

    if (tx->state == NTH_TX_CONNECTING || tx->state == NTH_TX_SENDING ||
        tx->state == NTH_TX_RECEIVING) {

        tx->prevState = tx->state;
        tx->state     = NTH_TX_FAILED;
        tx->lastError = NTH_ERR_DISCONNECTED;
    }
}

void nth_releaseTransaction(NthTransaction* tx) {
    RETURN_IF_NULL(tx, ;);
    NTH_LOG("nth: releasing transaction ...");
    nth_disconnect(tx);
    tx->active = false;
    memset(tx, 0, sizeof(*tx));
}

static void nth_closeSocketOnly(NthTransaction* tx) {
    RETURN_IF_NULL(tx, ;);

    if (tx->socketFd >= 0) {
        NTH_LOG("nth: closing connection socket = %d", tx->socketFd);

        g_transport->close(tx->socketFd);
    }

    tx->socketFd = NTH_SOCKET_CLOSE;
}

static void nth_scheduleConnectRetry(NthTransaction* tx, NthResult finalError) {
    RETURN_IF_NULL(tx, ;);

    nth_closeSocketOnly(tx);

    if (tx->connectAttempt < NTH_CONNECT_MAX_ATTEMPTS) {

        tx->connectRetryPending = true;

        tx->connectRetryTick = nth_getTick() + NTH_CONNECT_RETRY_DELAY_MS;

        /*
         * Reset timeout reference so nth_checkTimeout()
         * doesn't immediately timeout while waiting
         * for retry.
         */
        tx->startTick = nth_getTick();

        tx->state = NTH_TX_CONNECTING;

        NTH_LOG("nth: connect attempt %d/%d failed; retry %d in %d ms",
                tx->connectAttempt, NTH_CONNECT_MAX_ATTEMPTS,
                tx->connectAttempt + 1, NTH_CONNECT_RETRY_DELAY_MS);

        return;
    }

    NTH_LOG("nth: connect failed after %d attempts", tx->connectAttempt);

    nth_fail(tx, finalError);
}

static NthResult nth_startConnectAttempt(NthTransaction* tx) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);

    tx->connectRetryPending = false;

    tx->connectAttempt++;

    tx->startTick = nth_getTick();

    NTH_LOG("nth: connect attempt %d/%d to %s:%d", tx->connectAttempt,
            NTH_CONNECT_MAX_ATTEMPTS, tx->connectHost, tx->connectPort);

    tx->socketFd = g_transport->connect(tx->connectHost, tx->connectPort);

    NTH_LOG("nth: connect returned socket = %d", tx->socketFd);

    if (tx->socketFd < 0) {

        tx->socketFd = NTH_SOCKET_CLOSE;

        nth_scheduleConnectRetry(tx, NTH_ERR_CONNECT);

        /*
         * The operation has still been accepted asynchronously
         * if another retry remains.
         */
        if (tx->state != NTH_TX_FAILED)
            return NTH_OK;

        return NTH_ERR_CONNECT;
    }

    tx->state = NTH_TX_CONNECTING;

    return NTH_OK;
}

static void nth_complete(NthTransaction* tx) {
    RETURN_IF_NULL(tx, ;);

    tx->prevState = tx->state;
    tx->state     = NTH_TX_COMPLETED;

    NTH_LOG("nth: transaction completed, socket = %d", tx->socketFd);

    /*
     * Transaction is complete. We don't need the
     * connection anymore.
     */
    nth_closeSocketOnly(tx);

    if (tx->onReceive != NULL) {
        tx->onReceive(tx, tx->userData);
    }
}

NthResult nth_connect(NthTransaction* tx, const char* host, uint16_t port) {

    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);

    RETURN_VALUE_IF_NULL(host, ;, NTH_ERR_INVALID_ARG);

    RETURN_VALUE_IF_NOT(isValidHost(host), true, ;, NTH_ERR_INVALID_HOST);

    if (tx->socketFd >= 0 || tx->state == NTH_TX_CONNECTING) {

        return NTH_ERR_INVALID_STATE;
    }

    if (strlen(host) >= sizeof(tx->connectHost)) {
        return NTH_ERR_INVALID_HOST;
    }

    /*
     * Store destination because retries happen asynchronously
     * from nth_tick().
     */
    strncpy(tx->connectHost, host, sizeof(tx->connectHost) - 1);

    tx->connectHost[sizeof(tx->connectHost) - 1] = '\0';

    tx->connectPort = port;

    tx->connectAttempt      = 0;
    tx->connectRetryPending = false;
    tx->connectRetryTick    = 0;

    tx->state = NTH_TX_CONNECTING;

    /*
     * Start attempt #1.
     *
     * Attempts #2 and #3 will be started later
     * through nth_tick().
     */
    return nth_startConnectAttempt(tx);
}

NthResult nth_send(NthTransaction* tx, ByteArray* ba) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);
    RETURN_VALUE_IF_NULL(ba, ;, NTH_ERR_INVALID_ARG);
    RETURN_VALUE_IF_NULL(ba->data, ;, NTH_ERR_INVALID_ARG);

    if (tx->socketFd < 0) {
        nth_fail(tx, NTH_ERR_DISCONNECTED);
        return NTH_ERR_DISCONNECTED;
    }

    tx->startTick = nth_getTick();

    if (ba->len > tx->txBuffer.capacity) {
        NTH_LOG("NTH: buffer overfllow.data len = %d, buffer capacity = %d",
                ba->len, tx->txBuffer.capacity);
        nth_fail(tx, NTH_ERR_OVERFLOW);
        return NTH_ERR_OVERFLOW;
    }
    memcpy(tx->txBuffer.data, ba->data, ba->len);

    tx->txBuffer.len = ba->len;

    tx->txOffset = 0;

    tx->state = NTH_TX_SENDING;
    return NTH_OK;
}

NthResult nth_setTx(NthTransaction* tx, ByteArray* ba) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);
    RETURN_VALUE_IF_NULL(ba, ;, NTH_ERR_INVALID_ARG);
    RETURN_VALUE_IF_NULL(ba->data, ;, NTH_ERR_INVALID_ARG);

    if (ba->len > tx->txBuffer.capacity) {
        NTH_LOG("NTH: buffer overfllow.data len = %d, buffer capacity = %d",
                ba->len, tx->txBuffer.capacity);
        nth_fail(tx, NTH_ERR_OVERFLOW);
        return NTH_ERR_OVERFLOW;
    }

    memcpy(tx->txBuffer.data, ba->data, ba->len);

    tx->txBuffer.len = ba->len;
    return NTH_OK;
}

NthResult nth_sendProvidedTx(NthTransaction* tx) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);

    if (tx->socketFd < 0) {
        nth_fail(tx, NTH_ERR_DISCONNECTED);
        return NTH_ERR_DISCONNECTED;
    }
    tx->txOffset  = 0;
    tx->state     = NTH_TX_SENDING;
    tx->startTick = nth_getTick();
    return NTH_OK;
}

static void nth_handleConnecting(NthTransaction* tx) {
    RETURN_IF_NULL(tx, ;);

    uint32_t now = nth_getTick();

    /*
     * We are between attempts.
     *
     * nth_tick() keeps coming here, but we don't block.
     */
    if (tx->connectRetryPending) {

        /*
         * Signed subtraction makes this safe across
         * uint32 tick wraparound.
         */
        if ((int32_t)(now - tx->connectRetryTick) < 0)
            return;

        NTH_LOG("nth: starting retry attempt %d", tx->connectAttempt + 1);

        nth_startConnectAttempt(tx);

        return;
    }

    /*
     * A real connection attempt must have a descriptor.
     */
    if (tx->socketFd < 0) {

        nth_scheduleConnectRetry(tx, NTH_ERR_CONNECT);

        return;
    }

    int ret = g_transport->poll(tx->socketFd, 0);

    if (ret > 0) {

        NTH_LOG("nth: socket connected on attempt %d", tx->connectAttempt);

        tx->connectRetryPending = false;

        tx->state = NTH_TX_READY;

        if (tx->onConnect) {
            NTH_LOG("nth: calling connect callback.");

            tx->onConnect(tx, tx->userData);
        }

        return;
    }

    if (ret < 0) {

        NTH_LOG("nth: socket connect poll failed on attempt %d",
                tx->connectAttempt);

        nth_scheduleConnectRetry(tx, NTH_ERR_CONNECT);

        return;
    }

    /*
     * ret == 0:
     *
     * Still connecting. Do nothing.
     * nth_tick() will poll again later.
     */
}

static void nth_handleSending(NthTransaction* tx) {
    RETURN_IF_NULL(tx, ;);

    size_t remain = tx->txBuffer.len - tx->txOffset;

    int32_t ret = g_transport->send(tx->socketFd,
                                    tx->txBuffer.data + tx->txOffset, remain);

    if (ret == 0) {
        return;
    }
    NTH_LOG("nth: tx->socketFd = %d, tx->txOffset = %d, remian %d",
            tx->socketFd, tx->txOffset, remain);
    if (ret < 0) {
        NTH_LOG("nth: sending data failed. sent data len = %d", ret);
        nth_fail(tx, NTH_ERR_SEND);
        return;
    }

    NTH_LOG("nth: send %d bytes succeed.", ret);

    tx->txOffset += ret;

    if (tx->txOffset >= tx->txBuffer.len) {
        nth_resetRx(tx);
        tx->state     = NTH_TX_RECEIVING;
        tx->startTick = nth_getTick();
        if (tx->onSent) {
            NTH_LOG("nth: calling send callback.");
            tx->onSent(tx, tx->userData);
        }
        // nth_emitSendEvent(tx);
        nth_resetTx(tx);
        NTH_LOG("nth: sending data succeed.");
    }
}

static void nth_handleReceiving(NthTransaction* tx) {
    size_t        remain;
    size_t        received = 0U;
    size_t        chunkOffset;
    NthIoStatus   ioStatus;
    NthRxDecision decision;

    RETURN_IF_NULL(tx, ;);

    if (tx->streamRx && tx->onChunk == NULL) {
        NTH_LOG("nth: streaming receive requires onChunk");
        nth_fail(tx, NTH_ERR_INTERNAL);
        return;
    }

    if (tx->rxOffset > tx->rxBuffer.capacity) {
        nth_fail(tx, NTH_ERR_INTERNAL);
        return;
    }

    remain = tx->rxBuffer.capacity - tx->rxOffset;

    if (remain == 0U) {
        nth_fail(tx, NTH_ERR_OVERFLOW);
        return;
    }

    chunkOffset = tx->rxOffset;

    ioStatus = g_transport->recv(tx->socketFd, tx->rxBuffer.data + tx->rxOffset,
                                 remain, &received);

    // LOG_TRACE("net receive io status = %d", ioStatus);

    switch (ioStatus) {

    case NTH_IO_WOULD_BLOCK: {
        /* Temporary diagnostic; shared log throttle across transactions. */
        static uint32_t lastLogTick = 0U;
        uint32_t        now         = nth_getTick();
        uint32_t        elapsed     = (uint32_t)(now - tx->startTick);

        if ((uint32_t)(now - lastLogTick) >= 5000U) {
            lastLogTick = now;

            LOG_TRACE("NTH waiting: fd=%d now=%lu start=%lu "
                      "elapsed=%lu timeout=%lu",
                      tx->socketFd, (unsigned long)now,
                      (unsigned long)tx->startTick, (unsigned long)elapsed,
                      (unsigned long)tx->timeoutMs);
        }

        return;
    }

    case NTH_IO_ERROR:
        NTH_LOG("nth: receiving data failed.");
        nth_fail(tx, NTH_ERR_RECEIVE);
        return;

    case NTH_IO_CLOSED:

        NTH_LOG("nth: peer closed connection.");

        tx->peerClosed = true;
        if (tx->isComplete == NULL) {
            nth_fail(tx, NTH_ERR_DISCONNECTED);
            return;
        }

        decision = tx->isComplete(tx, tx->userData);

        NTH_LOG("nth: tx->isComplete() -> decisioin = %d", decision);

        /*
         * No additional bytes can arrive after EOF.
         */
        if (decision != NTH_RX_COMPLETE) {
            nth_fail(tx, NTH_ERR_PROTOCOL);
            return;
        }
        nth_complete(tx);

        return;

    case NTH_IO_DATA:
        break;

    default:
        nth_fail(tx, NTH_ERR_INTERNAL);
        return;
    }

    LOG_TRACE("net receive: received data len = %lu", received);

    RETURN_IF(received, 0, nth_fail(tx, NTH_ERR_PROTOCOL););

    RETURN_IF_GREATER(received, remain, nth_fail(tx, NTH_ERR_PROTOCOL););

    tx->rxOffset += received;
    tx->rxBuffer.len = tx->rxOffset;
    /*
     * Notify observers about only the newly received bytes.
     */
    if (tx->onChunk != NULL) {
        ByteArray chunk;

        chunk.data     = tx->rxBuffer.data + chunkOffset;
        chunk.len      = received;
        chunk.capacity = received;

        tx->onChunk(tx, &chunk, tx->userData);

        /*
         * The callback may have cancelled the transaction.
         */
        if (tx->state != NTH_TX_RECEIVING)
            return;
    }

    if (tx->isComplete == NULL) {
        nth_fail(tx, NTH_ERR_INTERNAL);
        return;
    }

    decision = tx->isComplete(tx, tx->userData);

    switch (decision) {

    case NTH_RX_MORE:
        if (tx->streamRx) {
            /*
             * onChunk and isComplete have finished processing
             * these bytes. Reuse the buffer for the next receive.
             *
             * Do not retain pointers into rxBuffer/onChunk data.
             */
            tx->rxOffset     = 0U;
            tx->rxBuffer.len = 0U;
        }
        return;

    case NTH_RX_COMPLETE:

        nth_complete(tx);

        return;

    case NTH_RX_ERROR:
    default:
        nth_fail(tx, NTH_ERR_PROTOCOL);
        return;
    }
}

static void nth_checkTimeout(NthTransaction* tx) {
    RETURN_IF_NULL(tx, ;);

    switch (tx->state) {

    case NTH_TX_CONNECTING:
    case NTH_TX_SENDING:
    case NTH_TX_RECEIVING:
        break;

    default:
        return;
    }

    /*
     * We're intentionally waiting before the next
     * connection attempt.
     */
    if (tx->state == NTH_TX_CONNECTING && tx->connectRetryPending) {

        return;
    }

    uint32_t now = nth_getTick();

    if ((now - tx->startTick) < tx->timeoutMs)
        return;

    /*
     * CONNECT timeout:
     *
     * Retry attempts #2/#3 instead of immediately
     * failing the whole transaction.
     */
    if (tx->state == NTH_TX_CONNECTING &&
        tx->connectAttempt < NTH_CONNECT_MAX_ATTEMPTS) {

        NTH_LOG("nth: connect attempt %d timed out", tx->connectAttempt);

        nth_scheduleConnectRetry(tx, NTH_ERR_TIMEOUT);

        return;
    }

    /*
     * Final timeout.
     */
    NTH_LOG("nth: timeout occurred.");

    tx->connectRetryPending = false;

    /*
     * Very important:
     * Release the current socket before entering FAILED state.
     */
    nth_closeSocketOnly(tx);

    tx->prevState = tx->state;
    tx->state     = NTH_TX_FAILED;
    tx->lastError = NTH_ERR_TIMEOUT;

    if (tx->onTimeout) {
        NTH_LOG("nth: calling timeout callback.");
        tx->onTimeout(tx, tx->userData);
    }
}

static void nth_fail(NthTransaction* tx, NthResult error) {
    NTH_LOG("nth: transaction failed.");
    if (!tx)
        return;
    nth_disconnect(tx);
    tx->prevState = tx->state;
    tx->state     = NTH_TX_FAILED;
    tx->lastError = error;

    if (tx->onFailure) {
        NTH_LOG("nth: calling on failure callback.");
        tx->onFailure(tx, tx->userData);
    }
}

void nth_tick(void) {
    for (size_t i = 0; i < NT_MAX_TRANSACTIONS; i++) {

        NthTransaction* tx = &g_transactions[i];

        if (!tx->active)
            continue;

        switch (tx->state) {

        case NTH_TX_IDLE:
            // NTH_LOG("nth: state idle.");
            break;

        case NTH_TX_READY:
            NTH_LOG("nth: state ready.");
            break;

        case NTH_TX_COMPLETED:
            NTH_LOG("nth: state completed.");
            break;

        case NTH_TX_CONNECTING:
            nth_handleConnecting(tx);
            break;

        case NTH_TX_SENDING:
            nth_handleSending(tx);
            break;

        case NTH_TX_RECEIVING:
            nth_handleReceiving(tx);
            break;
        case NTH_TX_FAILED:
            break;
        default:
            break;
        }

        nth_checkTimeout(tx);
    }
}

OOP_CTOR(Nth) {
    self->alloc          = nth_allocTransaction;
    self->connect        = nth_connect;
    self->init           = nth_init;
    self->release        = nth_releaseTransaction;
    self->send           = nth_send;
    self->tick           = nth_tick;
    self->setTx          = nth_setTx;
    self->sendProvidedTx = nth_sendProvidedTx;
    self->disconnect     = nth_disconnect;
}

Nth* nth() {
    CALL_ONCE(OOP_CALL_CTOR(Nth, &__nth); nth_init(););
    return &__nth;
}