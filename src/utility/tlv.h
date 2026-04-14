#ifndef TLV_H_
#define TLV_H_

#include "oop.h"
#include <stdint.h>

typedef struct {
    const uint8_t *tag;
    uint16_t tagLen;
    const uint8_t *value;
    uint16_t valueLen;
} TlvItem;

typedef int (*TlvCallback)(const TlvItem *item, void *user);

OOP_CLASS(TLV) {
    OOP_METHOD(int, decode, const uint8_t *data,
              uint16_t len,
              TlvCallback cb,
              void *user);
    OOP_METHOD(uint16_t, encode, uint8_t *out,
                    const uint8_t *tag,
                    uint8_t tagLen,
                    const void *value,
                    uint16_t valueLen);
};

TLV *tlv();

#endif