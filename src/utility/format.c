#include "format.h"
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "logger.h"
#include "sys/sys.h"
#include "common.h"
#include "phrases/phrases.h"
#include <inttypes.h>
#include "len.h"

void timeFormat(const char* in, char* out) {
    char digits[6] = {0};
    int  dcount    = 0;

    // Extract digits (max 6)
    for (int i = 0; in[i] && dcount < 6; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[dcount++] = in[i];
        }
    }

    int pos = 0;
    int di  = 0;

    // HH
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = ':';

    // MM
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = ':';

    // SS
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos] = '\0';
}

void dateFormat(const char* in, char* out) {
    char digits[8] = {0};
    int  dcount    = 0;

    // Extract digits (max 8)
    for (int i = 0; in[i] && dcount < 8; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[dcount++] = in[i];
        }
    }

    int pos = 0;
    int di  = 0;

    // YYYY
    for (int i = 0; i < 4; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = '/';

    // MM
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = '/';

    // DD
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos] = '\0';
}

void getDateTimeUint(uint32_t* date, uint32_t* time) {
    DateTime* dt = OOP_CALL(sys(), getDateTime);
    Date_t    jd = getJalaliDate();
    int       hh, mm, ss;
    sscanf(dt->time, "%2d%2d%2d", &hh, &mm, &ss);
    *date = jd.year * 10000 + jd.month * 100 + jd.day;
    *time = hh * 10000 + mm * 100 + ss;
}

void dateTimeToStr(uint32_t date, uint32_t time, char* str, size_t size) {
    char dt[24] = {0};
    int  yy     = date / 10000;
    int  tmp    = (date % 10000);
    int  mm     = tmp / 100;
    int  dd     = tmp % 100;

    int hh  = time / 10000;
    tmp     = (time % 10000);
    int min = tmp / 100;
    int ss  = tmp % 100;
    snprintf(str, size, "%02d:%02d:%02d-%04d/%02d/%02d", hh, min, ss, yy, mm,
             dd);
}

void dateToStr(uint32_t date, char* str, size_t size) {
    int yy  = date / 10000;
    int tmp = (date % 10000);
    int mm  = tmp / 100;
    int dd  = tmp % 100;
    snprintf(str, size, "%04d/%02d/%02d", yy, mm, dd);
}

void dateToJalaliStr(uint32_t date, char* out, size_t size) {
    int yy  = date / 10000;
    int tmp = (date % 10000);
    int mm  = tmp / 100;
    int dd  = tmp % 100;

    int    full_year = 2000 + yy; // adjust if needed
    Date_t greg;
    greg.day   = dd;
    greg.month = mm;
    greg.year  = full_year;
    Date_t jalali;
    gregorianToJalali(greg, &jalali);
    snprintf(out, size, "%02d/%02d/%02d", jalali.year % 100, jalali.month,
             jalali.day);
}

void timeToStr(uint32_t time, char* str, size_t size) {
    int hh  = time / 10000;
    int tmp = (time % 10000);
    int min = tmp / 100;
    int ss  = tmp % 100;
    snprintf(str, size, "%02d:%02d:%02d", hh, min, ss);
}

void shortDateTimeToStr(uint32_t date, uint32_t time, char* str, size_t size) {
    int mm = (date % 10000) / 100;
    int dd = date % 100;

    int hh  = time / 10000;
    int min = (time % 10000) / 100;

    snprintf(str, size, "%02d/%02d-%02d/%02d", mm, dd, hh, min);
}

void extractDatetimeStr(const char* buf, char* date, char* time) {
    // Basic length check (optional, for safety)
    if (strlen(buf) < 14) {
        // invalid input, you can handle error as needed
        date[0] = '\0';
        time[0] = '\0';
        return;
    }

    // Copy "yyyymmdd"
    memcpy(date, buf, 8);
    date[8] = '\0';

    // Copy "hhmmss"
    memcpy(time, buf + 8, 6);
    time[6] = '\0';
}

void extractDatetimeInt(const char* buf, uint32_t* date, uint32_t* time) {
    char tmp[9]; // enough for "yyyymmdd" + '\0'

    if (strlen(buf) < 14) {
        *date = -1;
        *time = -1;
        return;
    }

    // Extract date (yyyymmdd)
    memcpy(tmp, buf, 8);
    tmp[8] = '\0';
    *date  = toInt(tmp);

    // Extract time (hhmmss)
    memcpy(tmp, buf + 8, 6);
    tmp[6] = '\0';
    *time  = toInt(tmp);
}

void formatDateTimeStr(char* date, char* day, size_t out_size) {
    DateTime* dt = OOP_CALL(sys(), getDateTime);

    int yy, mm, dd;
    sscanf(dt->date, "%2d%2d%2d", &yy, &mm, &dd);

    int full_year = 2000 + yy; // adjust if needed
    snprintf(day, out_size, "%s", getDayName(full_year, mm, dd));
    Date_t greg;
    greg.day   = dd;
    greg.month = mm;
    greg.year  = full_year;
    Date_t jalali;
    gregorianToJalali(greg, &jalali);
    snprintf(date, out_size, "%02d/%02d/%02d", jalali.year % 100, jalali.month,
             jalali.day);
}

void formatTimeStr(char* out, size_t out_size) {
    DateTime* dt = OOP_CALL(sys(), getDateTime);

    int hh, mm;
    sscanf(dt->time, "%2d%2d", &hh, &mm);

    snprintf(out, out_size, "%02d:%02d", hh, mm);
}

int amountSeparator(const char* in, char* out, size_t out_size) {
    size_t len;
    size_t commas;
    size_t out_len;
    size_t i, j;

    if (!in || !out)
        return -1;

    len = strlen(in);

    /* empty string → empty output */
    if (len == 0) {
        if (out_size > 0)
            out[0] = '\0';
        return 0;
    }

    /* validate digits only */
    for (i = 0; i < len; i++) {
        if (!isDigit((char)in[i])) {
            return -1;
        }
    }

    commas  = (len - 1) / 3;
    out_len = len + commas;
    if (out_size < out_len + 1)
        return -1;

    out[out_len] = '\0';

    /* build output from right to left */
    i         = len;
    j         = out_len;
    int count = 0;

    while (i > 0) {
        out[--j] = in[--i];
        count++;

        if (count == 3 && i > 0) {
            out[--j] = ',';
            count    = 0;
        }
    }

    return 0;
}

int timeSeparator(const char* in, char* out, size_t out_size) {
    if (!in || !out)
        return -1;

    if (strlen(in) != 6)
        return -1;

    for (int i = 0; i < 6; i++) {
        if (!isDigit((unsigned char)in[i]))
            return -1;
    }

    /* "HH:MM:SS" => 8 chars + null */
    if (out_size < 9)
        return -1;

    out[0] = in[0];
    out[1] = in[1];
    out[2] = ':';
    out[3] = in[2];
    out[4] = in[3];
    out[5] = ':';
    out[6] = in[4];
    out[7] = in[5];
    out[8] = '\0';

    return 0;
}

int dateSeparator(const char* in, char* out, size_t out_size) {
    size_t len;

    if (!in || !out)
        return -1;

    len = strlen(in);

    /* Accept only 6 or 8 digits */
    if (!(len == 6 || len == 8))
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (!isDigit((unsigned char)in[i]))
            return -1;
    }

    /* Output size: len + 2 separators + null */
    if (out_size < len + 3)
        return -1;

    if (len == 6) {
        /* YY/MM/DD */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = '/';
        out[3] = in[2];
        out[4] = in[3];
        out[5] = '/';
        out[6] = in[4];
        out[7] = in[5];
        out[8] = '\0';
    } else {
        /* YYYY/MM/DD */
        out[0]  = in[0];
        out[1]  = in[1];
        out[2]  = in[2];
        out[3]  = in[3];
        out[4]  = '/';
        out[5]  = in[4];
        out[6]  = in[5];
        out[7]  = '/';
        out[8]  = in[6];
        out[9]  = in[7];
        out[10] = '\0';
    }

    return 0;
}

void gregorianToJalali(Date_t greg, Date_t* jalali) {
    int array[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    if (greg.year <= 1600) {
        greg.year -= 621;
        jalali->year = 0;
    } else {
        greg.year -= 1600;
        jalali->year = 979;
    }

    int temp = (greg.year > 2) ? (greg.year + 1) : greg.year;
    int days = ((int)((temp + 3) / 4)) + (365 * greg.year) -
               ((int)((temp + 99) / 100)) - 80 + array[greg.month - 1] +
               ((int)((temp + 399) / 400)) + greg.day;
    jalali->year += 33 * ((int)(days / 12053));
    days %= 12053;
    jalali->year += 4 * ((int)(days / 1461));
    days %= 1461;

    if (days > 365) {
        jalali->year += (int)((days - 1) / 365);
        days = (days - 1) % 365;
    }

    jalali->month =
        (days < 186) ? 1 + (int)(days / 31) : 7 + (int)((days - 186) / 30);

    jalali->day = 1 + ((days < 186) ? (days % 31) : ((days - 186) % 30));
}

int gregorianToJalaliStr(const char* in_date, char* out_date) {
    int gy, gm, gd;
    int jy, jm, jd;

    if (!in_date || !out_date)
        return -1;

    /* must be exactly 6 digits */
    if (strlen(in_date) != 6)
        return -1;

    for (int i = 0; i < 6; i++) {
        if (!isDigit((unsigned char)in_date[i]))
            return -1;
    }

    /* parse YYMMDD safely */
    gy = (in_date[0] - '0') * 10 + (in_date[1] - '0');
    gy += 2000;

    gm = (in_date[2] - '0') * 10 + (in_date[3] - '0');
    gd = (in_date[4] - '0') * 10 + (in_date[5] - '0');

    /* basic range check */
    if (gm < 1 || gm > 12 || gd < 1 || gd > 31)
        return -1;

    /* assume this function already exists */
    Date_t greg;
    greg.day   = gd;
    greg.month = gm;
    greg.month = gy;
    Date_t jalali;
    gregorianToJalali(greg, &jalali);

    /* format as YYMMDD */
    sprintf(out_date, "%02d%02d%02d", jalali.year % 100, jalali.month,
            jalali.day);

    return 0;
}

Date_t getJalaliDate() {
    DateTime* dt = OOP_CALL(sys(), getDateTime);

    int yy, mm, dd;
    sscanf(dt->date, "%2d%2d%2d", &yy, &mm, &dd);

    int    full_year = 2000 + yy; // adjust if needed
    Date_t greg;
    greg.day   = dd;
    greg.month = mm;
    greg.year  = full_year;
    Date_t jalali;
    gregorianToJalali(greg, &jalali);
    return jalali;
}