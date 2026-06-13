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


#define ByteArray(name, size)                       \
    uint8_t data_##name[size] = {0};                \
    ByteArray name;                                 \
    name.data = data_##name;                       \
    name.capacity = size;                          \

#endif