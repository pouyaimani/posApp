#ifndef CONVERT_H_
#define CONVERT_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

int libAtoi(const char* str);

int intToStr(int val, char* out, size_t size);

int addBeHarf(const char* num, char* out, size_t out_size);

unsigned char intToChar(int val);

bool str2u64(const char* str, uint64_t* value);

bool u64toStr(uint64_t value, char* str, size_t size);

void convertStrNoNumber(char* source, int len, unsigned char hex,
                        int startCompare, int endCompare, char* dest);

int     toInt(const char* value);
int16_t hexCharToDecimal(char hexChar);
void    toPersianDigits(char* out, size_t out_size, int value);

void bytesToHex(const uint8_t* data, size_t dataLen, char* out, size_t outSize);
void hexStringToBytes(char* inhex, int len, unsigned char* retval);

int hex2data(unsigned char* data, const unsigned char* hexstring,
             unsigned int len);
typedef enum {
    TYPE_INT8,
    TYPE_UINT8,
    TYPE_INT16,
    TYPE_UINT16,
    TYPE_INT32,
    TYPE_UINT32,
    TYPE_INT64,
    TYPE_UINT64
} NumberType;

bool stringToNumber(const char* str, void* out, NumberType type);

bool numberToString(const void* value, NumberType type, char* buffer,
                    uint32_t size);

#define STRING_TO_U8(s, v)  stringToNumber(s, v, TYPE_UINT8)
#define STRING_TO_U16(s, v) stringToNumber(s, v, TYPE_UINT16)
#define STRING_TO_U32(s, v) stringToNumber(s, v, TYPE_UINT32)
#define STRING_TO_U64(s, v) stringToNumber(s, v, TYPE_UINT64)

#define STRING_TO_I8(s, v)  stringToNumber(s, v, TYPE_INT8)
#define STRING_TO_I16(s, v) stringToNumber(s, v, TYPE_INT16)
#define STRING_TO_I32(s, v) stringToNumber(s, v, TYPE_INT32)
#define STRING_TO_I64(s, v) stringToNumber(s, v, TYPE_INT64)

#define U8_TO_STRING(v, b)  numberToString(v, TYPE_UINT8, b, sizeof(b))
#define U16_TO_STRING(v, b) numberToString(v, TYPE_UINT16, b, sizeof(b))
#define U32_TO_STRING(v, b) numberToString(v, TYPE_UINT32, b, sizeof(b))
#define U64_TO_STRING(v, b) numberToString(v, TYPE_UINT64, b, sizeof(b))

#define I8_TO_STRING(v, b)  numberToString(v, TYPE_INT8, b, sizeof(b))
#define I16_TO_STRING(v, b) numberToString(v, TYPE_INT16, b, sizeof(b))
#define I32_TO_STRING(v, b) numberToString(v, TYPE_INT32, b, sizeof(b))
#define I64_TO_STRING(v, b) numberToString(v, TYPE_INT64, b, sizeof(b))

#endif