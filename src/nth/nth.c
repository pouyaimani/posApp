#include "nth.h"
#include <string.h>
#include "event.h"
#include "core.h"
#include "error.h"

static Nth __nth;

extern NthTransport sysTransport;

static NthTransaction g_transactions[
    NT_MAX_TRANSACTIONS];

static NthTransport *g_transport;

static bool isValidIPv4(const char *ip) {
    RETURN_VALUE_IF_NULL(ip, ;, false);

    int num = 0;
    int dots = 0;
    int digits = 0;

    while (*ip)
    {
        if (*ip == '.')
        {
            // Empty section or too many dots
            if (digits == 0)
                return false;

            // Reset for next octet
            dots++;
            digits = 0;
            num = 0;

            if (dots > 3)
                return false;
        }
        else if (isDigit((unsigned char)*ip))
        {
            num = num * 10 + (*ip - '0');
            digits++;

            // Maximum value for IPv4 octet
            if (num > 255)
                return false;

            // Prevent numbers like 0000 or very long octets
            if (digits > 3)
                return false;
        }
        else
        {
            return false;
        }

        ip++;
    }

    // Must end with valid octet and exactly 3 dots
    return (dots == 3 && digits > 0);
}

static uint32_t nth_getTick(void) {
    return NTH_GET_TICK();
}

void nth_init() {
    g_transport = &sysTransport;

    memset(g_transactions,
           0,
           sizeof(g_transactions));
}

NthTransaction *nth_allocTransaction(void) {
    for (size_t i = 0;
         i < NT_MAX_TRANSACTIONS;
         i++) {

        NthTransaction *tx = &g_transactions[i];

        if (!tx->active) {

            memset(tx,
                   0,
                   sizeof(*tx));
            tx->socketFd = -1;
            tx->active = true;

            tx->txBuffer.data = tx->txStorage;
            tx->txBuffer.capacity =
                sizeof(tx->txStorage);

            tx->rxBuffer.data = tx->rxStorage;
            tx->rxBuffer.capacity =
                sizeof(tx->rxStorage);

            tx->timeoutMs =
                NT_DEFAULT_TIMEOUT_MS;

            return tx;
        }
    }

    return NULL;
}

void nth_releaseTransaction(
    NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);

    if (tx->socketFd >= 0) {
        NTH_LOG("NTH: closing socekt = %d", tx->socketFd);
        g_transport->close(tx->socketFd);
    }

    memset(tx,
           0,
           sizeof(*tx));
    tx->socketFd = -1;
}

NthResult nth_connect(
    NthTransaction *tx,
    const char *host,
    uint16_t port) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);
    RETURN_VALUE_IF_NULL(host, ;, NTH_ERR_INVALID_ARG);
    if (!isValidIPv4(host)) {
        return NTH_ERR_INVALID_HOST;
    }

    tx->socketFd =
        g_transport->connect(host, port);
    NTH_LOG("NTH: connect to socket = %d", tx->socketFd);

    if (tx->socketFd <= 0) {
        tx->state = NTH_TX_FAILED;
        return NTH_ERR_CONNECT;
    }

    tx->state = NTH_TX_CONNECTING;

    tx->startTick = nth_getTick();

    return NTH_OK;
}

NthResult nth_send(NthTransaction *tx,
                    ByteArray *ba) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);
    RETURN_VALUE_IF_NULL(ba, ;, NTH_ERR_INVALID_ARG);

    if (ba->len > tx->txBuffer.capacity) {
        NTH_LOG("NTH: buffer overfllow.data len = %d, buffer capacity = %d",
             ba->len, tx->txBuffer.capacity);
        return NTH_ERR_OVERFLOW;
    }

    memcpy(tx->txBuffer.data,
           ba->data,
           ba->len);

    tx->txBuffer.len = ba->len;

    tx->txOffset = 0;

    tx->state = NTH_TX_SENDING;
    tx->startTick = nth_getTick();
    return NTH_OK;
}

NthResult nth_setTx(NthTransaction *tx,
                    ByteArray *ba) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);
    RETURN_VALUE_IF_NULL(ba, ;, NTH_ERR_INVALID_ARG);

    if (ba->len > tx->txBuffer.capacity) {
        NTH_LOG("NTH: buffer overfllow.data len = %d, buffer capacity = %d",
             ba->len, tx->txBuffer.capacity);
        return NTH_ERR_OVERFLOW;
    }

    memcpy(tx->txBuffer.data,
           ba->data,
           ba->len);

    tx->txBuffer.len = ba->len;
    return NTH_OK;
}


NthResult nth_sendProvidedTx(NthTransaction *tx) {
    RETURN_VALUE_IF_NULL(tx, ;, NTH_ERR_INVALID_ARG);
    tx->txOffset = 0;
    tx->state = NTH_TX_SENDING;
    tx->startTick = nth_getTick();
    return NTH_OK;
}

static void nth_emitConnectEvent(
    NthTransaction *tx,
    bool connected) {
    RETURN_IF_NULL(tx, ;);
    SocketConnectEvent *ev =
        createEvent(SM_EVENT_SOCKET_CONNECT);

    RETURN_IF_NULL(ev, ;);

    ev->isConnected = connected;

    ev->base.target = tx->owner;

    DISPATCH_EVENT(ev);
}

static void nth_emitSendEvent(
    NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);
    SocketSentEvent *ev =
        createEvent(SM_EVENT_SOCKET_SENT);

    RETURN_IF_NULL(ev, ;);
    ev->base.target = tx->owner;

    DISPATCH_EVENT(ev);
}

static void nth_emitReadEvent(
    NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);
    SocketReadyReadEvent *ev =
        createEvent(SM_EVENT_SOCKET_READY_READ);

    RETURN_IF_NULL(ev, ;);

    ev->ba.data = tx->rxBuffer.data;
    ev->ba.len = tx->rxBuffer.len;

    ev->base.target = tx->owner;

    DISPATCH_EVENT(ev);
}

static void nth_emitTimeout(
    NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);
    SocketTimeOutEvent *ev =
        createEvent(SM_EVENT_SOCKET_TIME_OUT);

    RETURN_IF_NULL(ev, ;);
    ev->base.target = tx->owner;

    DISPATCH_EVENT(ev);
}

static void nth_handleConnecting(
    NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);
    int ret;

    ret = g_transport->poll(
        tx->socketFd,
        0);

    if (ret > 0) {
        NTH_LOG("nth: socket is connected.");
        tx->state = NTH_TX_IDLE;
        if (tx->onConnect) {
            tx->onConnect(tx, tx->userData);
        }
        nth_emitConnectEvent(tx,
                            true);
    } else if (ret < 0) {
        NTH_LOG("nth: socket can not connect.");
        tx->state = NTH_TX_FAILED;
        tx->lastError = NTH_ERR_CONNECT;
        if (tx->onFailure) {
            tx->onFailure(tx, tx->userData);
        }
        nth_emitConnectEvent(tx,
                                false);
    }
}

static void nth_handleSending(
    NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);
    int ret;

    size_t remain;

    remain = tx->txBuffer.len -
             tx->txOffset;

    ret = g_transport->send(
        tx->socketFd,
        tx->txBuffer.data +
            tx->txOffset,
        remain);

    if (ret < 0) {
        NTH_LOG("nth: sending data failed.");
        tx->state = NTH_TX_FAILED;
        return;
    }

    tx->txOffset += ret;

    if (tx->txOffset >=
        tx->txBuffer.len) {

        tx->state = NTH_TX_RECEIVING;
        tx->startTick = nth_getTick();
        if (tx->onSent) {
            tx->onSent(tx, tx->userData);
        }
        nth_emitSendEvent(tx);
        NTH_LOG("nth: sending data succeed.");
    }
}

static void nth_handleReceiving(NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);
    int ret;

    size_t remain;

    remain = tx->rxBuffer.capacity -
             tx->rxOffset;

    ret = g_transport->recv(
        tx->socketFd,
        tx->rxBuffer.data +
            tx->rxOffset,
        remain);

    if (ret < 0) {
        NTH_LOG("nth: receiving data failed.");
        tx->state = NTH_TX_FAILED;
        if (tx->onFailure) {
            tx->onFailure(tx, tx->userData);
        }
        return;
    }

    if (ret == 0)
        return;

    tx->rxOffset += ret;

    tx->rxBuffer.len =
        tx->rxOffset;
    if(tx->onReceive) {
        tx->onReceive(tx, tx->userData);
    }
    nth_emitReadEvent(tx);

    tx->state = NTH_TX_COMPLETED;
    NTH_LOG("nth: receiving data succeed.");
}

static void nth_checkTimeout(
    NthTransaction *tx) {
    RETURN_IF_NULL(tx, ;);

    switch (tx->state) {
    case NTH_TX_CONNECTING:
    case NTH_TX_SENDING:
    case NTH_TX_RECEIVING:
        break;

    default:
        return;
    }
    uint32_t now;

    now = nth_getTick();

    if ((now - tx->startTick)
                > tx->timeoutMs) {
        NTH_LOG("nth: time out occured.");
        tx->prevState = tx->state;
        tx->state =
            NTH_TX_FAILED;

        tx->lastError =
            NTH_ERR_TIMEOUT;

        if (tx->onTimeout) {

            tx->onTimeout(
                tx,
                tx->userData);
        }

        nth_emitTimeout(tx);
    }
}

void nth_tick(void) {
    for (size_t i = 0;
         i < NT_MAX_TRANSACTIONS;
         i++) {

        NthTransaction *tx =
            &g_transactions[i];

        if (!tx->active)
            continue;

        switch (tx->state) {

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
            // tx->active = false;
            return;
        default:
            break;
        }

        nth_checkTimeout(tx);
    }
}

static NthResult nth_process(NthTransaction *tx,
                    ByteArray *dtx) {
    // SM_GOTO(tx->procState);
}

OOP_CTOR(Nth) {
    self->alloc = nth_allocTransaction;
    self->connect = nth_connect;
    self->init = nth_init;
    self->release = nth_releaseTransaction;
    self->send = nth_send;
    self->tick = nth_tick;
    self->setTx = nth_setTx;
    self->sendProvidedTx = nth_sendProvidedTx;
}

Nth *nth() {
    CALL_ONCE(
        OOP_CALL_CTOR(Nth, &__nth);
    );
    return &__nth;
}