#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "convert.h"
#include "format.h"

typedef enum {
    BILL_PUBLIC_SERVICE = 0,
    BILL_WATER,
    BILL_ELECTRICITY,
    BILL_GAS,
    BILL_TELEPHONE,
    BILL_MOBILE,
    BILL_MUNICIPALIT,
    BILL_TAX = 8,
    BILL_DRIVE_POLIC,
    BILL_INVALID,
} BillType_t;

int8_t getNameofDay(uint32_t date, char* out, size_t size);

inline void InsertDotIP(const char* ip, char* ip_add);

bool isDigit(char c);
bool isAlphabetic(char c);

int  appendChar(char* buf, size_t buf_size, char c);
int  deleteChar(char* buf);
void clearStr(char* buf);

/*
 * Combines two uint32_t values into a single uint64_t
 * date: yyyymmdd (high 32 bits)
 * time: hhmmss   (low 32 bits)
 * returns: uint64_t combined as (date << 32) | time
 */
uint64_t packDateTime(uint32_t date, uint32_t time);

/*
 * Extract date and time from uint64_t
 * dateTime: packed value
 * date: output pointer to uint32_t (YYYMMDD)
 * time: output pointer to uint32_t (HHMMSS)
 */
void unpackDateTime(const uint64_t* dateTime, uint32_t* date, uint32_t* time);

void leftPadZero(const char* src, char* dst, size_t dstSize, int width);

void prependZerosInt(uint64_t number, int totalWidth, char* output,
                     size_t outputSize);
void prependZerosUInt64(uint64_t number, int totalWidth, char* output,
                        size_t outputSize);

bool ipFormatLeftAligned(const char* in, char* out);

bool normalizeIp(const char* input, char* output, size_t outputSize);

void bytesToHex(const uint8_t* data, size_t dataLen, char* out, size_t outSize);
void hexStringToBytes(char* inhex, int len, unsigned char* retval);

int hex2data(unsigned char* data, const unsigned char* hexstring,
             unsigned int len);

bool dateValidate(const char* in);

bool timeValidate(const char* in);

char* skipLeadingZeros(char* str);

bool removeLeadingZeros(const char* input, char* output, size_t outputSize);

/**
 * Extract BIN/IIN from PAN.
 *
 * @param pan      Null-terminated PAN string.
 * @param bin      Output buffer.
 * @param binSize  Size of output buffer.
 * @param binLen   Number of digits to extract (typically 6 or 8).
 *
 * @return true on success, false on invalid input.
 */
bool extractBin(const char* pan, char* bin, size_t binSize, size_t binLen);

int maskPan(const char* pan, char* masked, size_t masked_size);

void padRight(const char* unpadded, int unpadlength, int len, char* padded,
              char p);

void padLeft(const char* unpadded, int unpadlength, int len, char* padded,
             char p);

int getBillType(const char* billId, size_t len);

const char* getBillOrgName(BillType_t id);

bool isBillIdValid(const char* billId);

bool isBillValid(const char* billId, const char* paymentId);

bool billExtractAmount(const char* paymentId, char* amount, size_t alen);

#endif