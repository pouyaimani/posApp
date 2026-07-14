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

struct NthTransaction;

typedef int8_t (*NthCallback)(struct NthTransaction* tx, void* userData);

typedef struct NthTransaction {

    bool active;

    int socketFd;

    NthTxState state;
    NthTxState prevState;
    ByteArray  txBuffer;
    ByteArray  rxBuffer;

    uint8_t txStorage[NT_TX_BUFFER_SIZE];
    uint8_t rxStorage[NT_RX_BUFFER_SIZE];

    size_t txOffset;
    size_t rxOffset;

    uint32_t timeoutMs;
    uint32_t startTick;

    State* owner;

    void* userData;

    NthCallback onConnect;
    NthCallback onReceive;
    NthCallback onFailure;
    NthCallback onTimeout;
    NthCallback onSent;
    NthCallback isComplete;

    int lastError;

} NthTransaction;

OOP_CLASS(Nth) {
    OOP_METHOD(void, init);
    OOP_METHOD(NthTransaction*, alloc);
    OOP_METHOD(NthResult, start, NthTransaction*, ByteArray * dtx);
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