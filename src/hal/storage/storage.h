#ifndef STORAGE_H_
#define STORAGE_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"
#include "event.h"
#include "common.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define BEGIN_DSC_ARRAY \
    enum { __dsc_base = __COUNTER__ + 1 }

#define NEXT_PROP STR(__COUNTER__ - __dsc_base)

#define DSC_BYTE(field, def) \
    { NEXT_PROP, T_BYTE, (0), (sizeof(field)), (def), &(field) }

#define DSC_INT(field, def) \
    { NEXT_PROP, T_INT, (0), (sizeof(field)), (def), &(field) }

#define DSC_STR_BUF(field, def) \
    { NEXT_PROP, T_STRING, (0), sizeof(field) - 1, (def), (field) }

#define DSC_BIN(field, min, max, def) \
    { NEXT_PROP, T_BINARY, (min), (max), (def), (void*)(field) }

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