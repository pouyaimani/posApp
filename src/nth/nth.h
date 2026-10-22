#ifndef TN_TRANSACTION_H
#define TN_TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>

#include "oop.h"
#include "state.h"
#include "nth_config.h"
#include "nth_transport.h"
#include "nth_types.h"
#include "byteArray.h"

#define NTH_SOCKET_CLOSE           -1
#define NTH_CONNECT_MAX_ATTEMPTS   3
#define NTH_CONNECT_RETRY_DELAY_MS 300
#define NTH_MAX_HOST_LEN           64

struct NthTransaction;

typedef enum { NTH_RX_MORE = 0, NTH_RX_COMPLETE, NTH_RX_ERROR } NthRxDecision;

typedef void (*NthReceiveChunkCallback)(struct NthTransaction* tx,
                                        ByteArray* chunk, void* userData);

typedef NthRxDecision (*NthCompletionCallback)(struct NthTransaction* tx,
                                               void*                  userData);

typedef int8_t (*NthCallback)(struct NthTransaction* tx, void* userData);

typedef struct NthTransaction {

    bool active;

    int socketFd;

    NthTxState state;
    NthTxState prevState;
    ByteArray  txBuffer;
    ByteArray  rxBuffer;

    uint8_t ioStorage[NT_IO_BUFFER_SIZE];

    // uint8_t* txStorage;
    // uint8_t* rxStorage;

    size_t txOffset;
    size_t rxOffset;

    /*
     * false:
     *   Accumulate the complete response in rxBuffer.
     *
     * true:
     *   Deliver bytes through onChunk, then reuse rxBuffer while
     *   waiting for more data. The application/parser owns any
     *   information that must survive the callback.
     *
     * Requires onChunk.
     */
    bool streamRx;

    bool peerClosed;

    uint32_t timeoutMs;
    uint32_t startTick;

    State* owner;

    void* userData;

    NthCallback             onConnect;
    NthCallback             onReceive;
    NthCallback             onFailure;
    NthCallback             onTimeout;
    NthCallback             onSent;
    NthCompletionCallback   isComplete;
    NthReceiveChunkCallback onChunk;

    NthResult lastError;

    char     connectHost[NTH_MAX_HOST_LEN];
    uint16_t connectPort;

    uint8_t  connectAttempt;
    bool     connectRetryPending;
    uint32_t connectRetryTick;

} NthTransaction;

OOP_CLASS(Nth) {
    OOP_METHOD(void, init);
    OOP_METHOD(NthTransaction*, alloc);
    OOP_METHOD(NthResult, connect, NthTransaction * tx, const char* host,
               uint16_t port);
    OOP_METHOD(NthResult, send, NthTransaction * tx, ByteArray * ba);
    OOP_METHOD(NthResult, sendProvidedTx, NthTransaction * tx);
    OOP_METHOD(NthResult, setTx, NthTransaction * tx, ByteArray * ba);
    OOP_METHOD(void, release, NthTransaction* tx);
    OOP_METHOD(void, disconnect, NthTransaction* tx);
    OOP_METHOD(void, tick);
};

Nth* nth();

#endif