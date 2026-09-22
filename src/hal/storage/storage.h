#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "oop.h"

/*
 * Values are stored as raw bytes to remain compatible with the existing
 * settings files. Integer widths are now explicit; the loader also accepts
 * the legacy four-byte T_INT representation.
 */
typedef enum { T_U8, T_U16, T_U32, T_U64, T_BOOL, T_STRING, T_BINARY } DataType;

typedef struct {
    const char* key;
    DataType    type;
    uint16_t    minLen;
    uint16_t    maxLen;
    const char* defaultValue;
    void*       address;
} DataDescriptor;

/* Keep old source code buildable while new code moves to explicit types. */
#define T_BYTE T_U8
#define T_INT  T_U32

#define DSC_U8_NAMED(key_, item_, default_)                                    \
    {(key_), T_U8, sizeof(item_), sizeof(item_), (default_), &(item_)}

#define DSC_U16_NAMED(key_, item_, default_)                                   \
    {(key_), T_U16, sizeof(item_), sizeof(item_), (default_), &(item_)}

#define DSC_U32_NAMED(key_, item_, default_)                                   \
    {(key_), T_U32, sizeof(item_), sizeof(item_), (default_), &(item_)}

#define DSC_U64_NAMED(key_, item_, default_)                                   \
    {(key_), T_U64, sizeof(item_), sizeof(item_), (default_), &(item_)}

#define DSC_BOOL_NAMED(key_, item_, default_)                                  \
    {(key_), T_BOOL, sizeof(item_), sizeof(item_), (default_), &(item_)}

/* item_ must be a fixed-size char array, not a char pointer. */
#define DSC_STR_NAMED(key_, item_, default_)                                   \
    {(key_),     T_STRING,      0u, (uint16_t)(sizeof(item_) - 1u),            \
     (default_), (void*)(item_)}

#define DSC_BIN_NAMED(key_, item_)                                             \
    {(key_), T_BINARY, 0u, (uint16_t)sizeof(item_), NULL, (void*)(item_)}

/* Compatibility helpers for existing descriptor tables. */
#define DSC_BYTE(item_, default_)    DSC_U8_NAMED(#item_, item_, default_)
#define DSC_INT(item_, default_)     DSC_U32_NAMED(#item_, item_, default_)
#define DSC_STR_BUF(item_, default_) DSC_STR_NAMED(#item_, item_, default_)
#define DSC_BIN(item_, min_, max_, default_)                                   \
    {#item_,           T_BINARY,   (uint16_t)(min_),                           \
     (uint16_t)(max_), (default_), (void*)(item_)}

/* Retained for source compatibility with the old settings table. */
#define BEGIN_DSC_ARRAY

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
