#ifndef TLV_H_
#define TLV_H_

#include "oop.h"
#include <stdint.h>

typedef enum {
    TLV_OK = 0,

    // Generic errors
    TLV_ERR_INVALID_PARAM,
    TLV_ERR_BUFFER_TOO_SMALL,

    // Decode errors
    TLV_ERR_TAG_LENGTH_INVALID,
    TLV_ERR_TAG_OVERFLOW,
    TLV_ERR_LENGTH_MISSING,
    TLV_ERR_LENGTH_INVALID,
    TLV_ERR_LENGTH_OVERFLOW,
    TLV_ERR_VALUE_OVERFLOW,
    TLV_ERR_CALLBACK_FAILED

} TlvError_t;

typedef struct {
    const uint8_t* tag;
    uint16_t       tagLen;
    const uint8_t* value;
    uint16_t       valueLen;
} TlvItem;

typedef int (*TlvCallback)(const TlvItem* item, void* user);

OOP_CLASS(TLV) {
    OOP_METHOD(TlvError_t, decode, const uint8_t* data, uint16_t len,
               TlvCallback cb, void* user);
    OOP_METHOD(TlvError_t, encode, uint8_t* out, uint16_t outSize,
               const uint8_t* tag, uint8_t tagLen, const void* value,
               uint16_t valueLen, uint16_t* encodedLen);
};

TLV* tlv();

#endif