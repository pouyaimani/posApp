#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdlib.h>

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

#endif