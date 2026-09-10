#ifndef FORMAT_H_
#define FORMAT_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "convert.h"

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

int amountSeparator(const char* in, char* out, size_t out_size);

void formatJalDateTimeStr(char* date, char* day, size_t out_size);

void dateTimeToStrJal(uint32_t date, uint32_t time, char* str, size_t size);

void dateTimeToStrJalShort(uint32_t date, uint32_t time, char* str,
                           size_t size);

// Date time utilities
void formatDateTimeStr(char* date, char* day, size_t out_size);

void formatTimeStr(char* out, size_t out_size);

int dateSeparator(const char* in, char* out, size_t out_size);

int timeSeparator(const char* in, char* out, size_t out_size);

void gregorianToJalali(Date_t greg, Date_t* jalali);

int gregorianToJalaliStr(const char* in_date, char* out_date);

void dateTimeToInt(uint32_t* date, uint32_t* time);

void dateTimeToStr(uint32_t date, uint32_t time, char* str, size_t size);

void extractDatetimeStr(const char* buf, char* date, char* time);

void shortDateTimeToStr(uint32_t date, uint32_t time, char* str, size_t size);

void extractDatetimeInt(const char* buf, uint32_t* date, uint32_t* time);

void dateFormat(const char* in, char* out);

void timeFormat(const char* in, char* out);

Date_t getJalaliDate();

void dateToJalaliStr(uint32_t date, char* str, size_t size);

void dateToStr(uint32_t date, char* str, size_t size);

void timeToStr(uint32_t time, char* str, size_t size);

bool isValidJalaliDate(Date_t date);

/*
 * Converts a validated Jalali date to Gregorian.
 *
 * Returns:
 *   0  success
 *  -1  invalid argument/date
 */
int jalaliToGregorian(Date_t jalali, Date_t* gregorian);

/*
 * Input:
 *   "14050619"
 *
 * Output:
 *   20260910
 *
 * Returns:
 *   0  success
 *  -1  invalid format/date
 */
int jalaliDateStrToGregorianUint(const char* in, uint32_t* out);

/*
 * Accepts:
 *   "1430"   -> 143000
 *   "143025" -> 143025
 *
 * Returns:
 *   0  success
 *  -1  invalid format/time
 */
int timeStrToUint(const char* in, uint32_t* out);

/*
 * Returns:
 *   < 0  dt1 < dt2
 *     0  dt1 == dt2
 *   > 0  dt1 > dt2
 */
int compareDateTimeUint(uint32_t date1, uint32_t time1, uint32_t date2,
                        uint32_t time2);

#endif