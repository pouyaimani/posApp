#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

int libAtoi(const char *str);

int addBeHarf(const char *num, char *out, size_t out_size);

typedef struct {
    int year;
    int month;
    int day;
} Calendar_t;

// Date formatter
void gregorianToJalali(Calendar_t greg, Calendar_t * jalali);
int gregorianToJalaliStr(const char *in_date, char *out_date);

unsigned char intToChar(int val);

inline void InsertDotIP(const char *ip, char* ip_add);

bool isDigit(char c);
bool isAlphabetic(char c);
int toInt(const char *value);
int16_t hexCharToDecimal(char hexChar);

int dateSeparator(const char *in, char *out, size_t out_size);
int timeSeparator(const char *in, char *out, size_t out_size);

#endif