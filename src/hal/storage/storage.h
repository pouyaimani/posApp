#ifndef STORAGE_H_
#define STORAGE_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"
#include "event.h"
#include "common.h"

typedef enum{
    T_INT = 0,
    T_BYTE,
    T_STRING,
    T_BINARY
} DataType;

typedef struct {
    const char *key;
    DataType type;
    uint16_t minLen;
    uint16_t maxLen;
    const char *defaultValue;
    void *address;
} DataDescriptor;

OOP_CLASS(Storage){
    OOP_METHOD(void, load, DataDescriptor *dsc, size_t itemsCount, const char *addr);
    OOP_METHOD(void, save, DataDescriptor *dsc, size_t itemsCount, const char *addr);
    OOP_METHOD(void, reset, DataDescriptor *dsc, size_t itemsCount, const char *addr);
};

OOP_CTOR(Storage);

Storage *storage(void);

#endif