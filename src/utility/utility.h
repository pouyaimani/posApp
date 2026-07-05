#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

int libAtoi(const char* str);

int intToStr(int val, char* out, size_t size);

int addBeHarf(const char* num, char* out, size_t out_size);

typedef struct {
    int year;
    int month;
    int day;
} Date_t;

typedef struct {
    int hour;
    int minute;
    int secound;
} Time_t;

int8_t getNameofDay(uint32_t date, char* out, size_t size);

unsigned char intToChar(int val);

inline void InsertDotIP(const char* ip, char* ip_add);

bool    isDigit(char c);
bool    isAlphabetic(char c);
int     toInt(const char* value);
int16_t hexCharToDecimal(char hexChar);
void    toPersianDigits(char* out, size_t out_size, int value);

int  appendChar(char* buf, size_t buf_size, char c);
int  deleteChar(char* buf);
void clearStr(char* buf);

int amountSeparator(const char* in, char* out, size_t out_size);

// Date time utilities
void formatDateTimeStr(char* date, char* day, size_t out_size);
void formatTimeStr(char* out, size_t out_size);
int  dateSeparator(const char* in, char* out, size_t out_size);
int  timeSeparator(const char* in, char* out, size_t out_size);
void gregorianToJalali(Date_t greg, Date_t* jalali);
int  gregorianToJalaliStr(const char* in_date, char* out_date);

Date_t getJalaliDate();

void dateTimeToInt(uint32_t* date, uint32_t* time);
void dateTimeToStr(uint32_t date, uint32_t time, char* str, size_t size);

void extractDatetimeStr(const char* buf, char* date, char* time);
void shortDateTimeToStr(uint32_t date, uint32_t time, char* str, size_t size);
void extractDatetimeInt(const char* buf, uint32_t* date, uint32_t* time);

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

void prependZerosInt(int number, int totalWidth, char* output,
                     size_t outputSize);
void prependZerosUInt64(uint64_t number, int totalWidth, char* output,
                        size_t outputSize);

bool ipFormatLeftAligned(const char* in, char* out);

bool normalizeIp(const char* input, char* output, size_t outputSize);

void bytesToHex(const uint8_t* data, size_t dataLen, char* out, size_t outSize);
void hexStringToBytes(char* inhex, int len, unsigned char* retval);

void convertStrNoNumber(char* source, int len, unsigned char hex,
                        int startCompare, int endCompare, char* dest);

int hex2data(unsigned char* data, const unsigned char* hexstring,
             unsigned int len);

void dateFormat(const char* in, char* out);

bool dateValidate(const char* in);

bool timeValidate(const char* in);

void timeFormat(const char* in, char* out);

char* skipLeadingZeros(char* str);

bool removeLeadingZeros(const char* input, char* output, size_t outputSize);

#endif