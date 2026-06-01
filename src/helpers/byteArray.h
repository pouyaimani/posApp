#ifndef BYTE_ARRAY_H_
#define BYTE_ARRAY_H_

#include "oop.h"
#include <stdint.h>

OOP_CLASS(ByteArray) {
    uint8_t *data;
    uint32_t len;
    uint32_t capacity;
};

int8_t byteArrayInit(ByteArray *ba, size_t size);
int8_t byteArrayDestroy(ByteArray *ba);
#define BYTE_ARRAY(name, size)                      \
    ByteArray *name = MEM_ALLOC(sizeof(ByteArray)); \
    name->data = MEM_ALLOC(size);                   \
    memset(name->data, 0, size);                    \
    name->capacity = size;                          \

#endif