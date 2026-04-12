#ifndef TRANSMITTER_H_
#define TRANSMITTER_H_

#include "states.h"
#include <stdbool.h>
#include "common.h"

/*************************** Iso transmitter ***********************/

OOP_CLASS(IsoTransmitter) {
    OOP_EXTENDS(State);
};

OOP_CTOR(IsoTransmitter, State *parent, const char *name);

#define STATE_TRNS_ISO getState(STATE_ID_ISO_TRANSMITTER)

/*************************** http ***********************/

typedef enum {
    HTTP_BODY_IDENTITY = 0,
    HTTP_BODY_CHUNKED
} HttpBodyType;

typedef struct {
    char host[64];
    char path[128];

    uint8_t *txBuf;
    uint32_t txLen;

    uint8_t *rxBuf;
    uint32_t rxLen;
    uint32_t rxMax;

    uint32_t headerSize;

    int statusCode;

    // body handling
    HttpBodyType bodyType;
    uint32_t contentLength;
    uint32_t bodyReceived;

    // chunked decoding
    uint32_t chunkSize;
    uint32_t chunkReceived;
    bool chunkHeaderParsed;

    // resume
    uint32_t rangeStart;

    bool headerParsed;

    void (*onBodyChunk)(uint8_t *data, uint32_t len);

} HttpContext;

OOP_CLASS(HttpTransmitter) {
    OOP_EXTENDS(State);
    HttpContext *httpCtx;
};

OOP_CTOR(HttpTransmitter, State *parent, const char *name);

#define STATE_TRNS_HTTP getState(STATE_ID_HTTP_TRANSMITTER)

#endif