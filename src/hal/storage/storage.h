#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "oop.h"

typedef enum { T_U8, T_U16, T_U32, T_U64, T_BOOL, T_STRING, T_BINARY } DataType;

/*
 * id is the stable persistent identifier written as two big-endian bytes.
 */
typedef struct {
    uint16_t    id;
    DataType    type;
    uint16_t    minLen;
    uint16_t    maxLen;
    const char* defaultValue;
    void*       address;
} DataDescriptor;

#define DSC_U8_ID(id_, item_, default_)                                        \
    {(uint16_t)(id_),         T_U8,       (uint16_t)sizeof(item_),             \
     (uint16_t)sizeof(item_), (default_), &(item_)}

#define DSC_U16_ID(id_, item_, default_)                                       \
    {(uint16_t)(id_),         T_U16,      (uint16_t)sizeof(item_),             \
     (uint16_t)sizeof(item_), (default_), &(item_)}

#define DSC_U32_ID(id_, item_, default_)                                       \
    {(uint16_t)(id_),         T_U32,      (uint16_t)sizeof(item_),             \
     (uint16_t)sizeof(item_), (default_), &(item_)}

#define DSC_U64_ID(id_, item_, default_)                                       \
    {(uint16_t)(id_),         T_U64,      (uint16_t)sizeof(item_),             \
     (uint16_t)sizeof(item_), (default_), &(item_)}

#define DSC_BOOL_ID(id_, item_, default_)                                      \
    {(uint16_t)(id_),         T_BOOL,     (uint16_t)sizeof(item_),             \
     (uint16_t)sizeof(item_), (default_), &(item_)}

/* item_ must be a fixed-size character array, not a pointer. */
#define DSC_STR_ID(id_, item_, default_)                                       \
    {(uint16_t)(id_), T_STRING,      0u, (uint16_t)(sizeof(item_) - 1u),       \
     (default_),      (void*)(item_)}

#define DSC_BIN_ID(id_, item_)                                                 \
    {(uint16_t)(id_),         T_BINARY, 0u,                                    \
     (uint16_t)sizeof(item_), NULL,     (void*)&(item_)}

typedef struct {
    OOP_METHOD(Error_t, save, const DataDescriptor* dsc, size_t itemsCount,
               const char* addr);
    OOP_METHOD(Error_t, load, const DataDescriptor* dsc, size_t itemsCount,
               const char* addr);
    OOP_METHOD(Error_t, reset, const DataDescriptor* dsc, size_t itemsCount,
               const char* addr);
} Storage;

Storage* storage(void);

#endif /* STORAGE_H_ */
