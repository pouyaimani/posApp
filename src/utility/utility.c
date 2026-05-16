#include "utility.h"
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "logger.h"
#include "sys/sys.h"
#include "common.h"
#include "phrases/phrases.h"

int libAtoi(const char *str) {
    int s = 0;
    bool falg = false;

    while (*str == ' ')
    {
        str++;
    }
    if (*str == '-' || *str == '+')
    {
        if (*str == '-')
            falg = true;
        str++;
    }
    while (*str >= '0' && *str <= '9')
    {
        s = s * 10 + *str - '0';
        str++;
        if (s < 0)
        {
            s = 2147483647;
            break;
        }
    }
    return s * (falg ? -1 : 1);
}

int intToStr(int val, char *out, size_t size) {
    memset(out, 0, size);
    return sprintf(out, "%d", val);
}

void removeNonDigits(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if (isdigit((unsigned char)src[i])) {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

void removeDots(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if (src[i] != '.') {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

const char *numToLiteral(int x)
{
    switch (x) {
    case 1: return phraseGetDef(PHRASE_ONE);
    case 2: return phraseGetDef(PHRASE_TWO);
    case 3: return phraseGetDef(PHRASE_THREE);
    case 4: return phraseGetDef(PHRASE_FOUR);
    case 5: return phraseGetDef(PHRASE_FIVE);
    case 6: return phraseGetDef(PHRASE_SIX);
    case 7: return phraseGetDef(PHRASE_SEVEN);
    case 8: return phraseGetDef(PHRASE_EIGHT);
    case 9: return phraseGetDef(PHRASE_NINE);
    default:
        return "";
    }
}

typedef struct {
    char   *buf;
    size_t  size;
    size_t  len;
} writer_t;

static const char * const yekan_words[] = {
    "", "یک", "دو", "سه", "چهار", "پنج", "شش", "هفت", "هشت", "نه",
    "ده", "یازده", "دوازده", "سیزده", "چهارده",
    "پانزده", "شانزده", "هفده", "هجده", "نوزده"
};

static const char * const dahgan_words[] = {
    "", "", "بیست", "سی", "چهل", "پنجاه",
    "شصت", "هفتاد", "هشتاد", "نود"
};

static const char * const sadgan_words[] = {
    "", "صد", "دویست", "سیصد", "چهارصد",
    "پانصد", "ششصد", "هفتصد", "هشتصد", "نهصد"
};

static const char * const scale_words[] = {
    "", "هزار", "میلیون", "میلیارد", "تریلیون"
};

static void w_append(writer_t *w, const char *s)
{
    while (*s && w->len + 1 < w->size) {
        w->buf[w->len++] = *s++;
    }
    w->buf[w->len] = '\0';
}

static void convert_3digits(writer_t *w, int n)
{
    int h = n / 100;
    int r = n % 100;

    if (h) {
        w_append(w, sadgan_words[h]);
        if (r) w_append(w, " و ");
    }

    if (r < 20) {
        if (r) w_append(w, yekan_words[r]);
    } else {
        int t = r / 10;
        int o = r % 10;

        w_append(w, dahgan_words[t]);
        if (o) {
            w_append(w, " و ");
            w_append(w, yekan_words[o]);
        }
    }
}

int addBeHarf(const char *num, char *out, size_t out_size)
{
    writer_t w = { out, out_size, 0 };
    int groups[5] = {0};  /* up to trillions */
    int group_count = 0;
    int negative = 0;

    if (!num || !out || out_size == 0)
        return -1;

    if (*num == '-') {
        negative = 1;
        num++;
    }

    /* validate */
    for (const char *p = num; *p; p++) {
        if (!isdigit((unsigned char)*p))
            return -1;
    }

    /* split into 3-digit groups */
    int len = strlen(num);
    int factor = 1;
    int acc = 0;

    for (int i = len - 1; i >= 0; i--) {
        acc += (num[i] - '0') * factor;
        factor *= 10;

        if (factor == 1000 || i == 0) {
            groups[group_count++] = acc;
            acc = 0;
            factor = 1;
        }
    }

    if (negative)
        w_append(&w, "منفی ");

    int first = 1;

    for (int i = group_count - 1; i >= 0; i--) {
        if (groups[i] == 0)
            continue;

        if (!first)
            w_append(&w, " و ");

        convert_3digits(&w, groups[i]);

        if (*scale_words[i]) {
            w_append(&w, " ");
            w_append(&w, scale_words[i]);
        }

        first = 0;
    }

    if (first)  /* number was zero */
        w_append(&w, "صفر");

    return 0;
}

void gregorianToJalali(Date_t greg, Date_t * jalali)
{
	int array[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

	if(greg.year <= 1600) {
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

	if(days > 365){
		jalali->year += (int)((days - 1) / 365);
		days = (days-1) % 365;
	}

	jalali->month = (days < 186)
							? 1 + (int)(days / 31)
							: 7 + (int)((days - 186) / 30);

	jalali->day = 1 + ((days < 186)
							? (days % 31)
							: ((days - 186) % 30));
}

int gregorianToJalaliStr(const char *in_date, char *out_date)
{
    int gy, gm, gd;
    int jy, jm, jd;

    if (!in_date || !out_date)
        return -1;

    /* must be exactly 6 digits */
    if (strlen(in_date) != 6)
        return -1;

    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)in_date[i]))
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
    greg.day = gd;
    greg.month = gm;
    greg.month = gy;
    Date_t jalali;
    gregorianToJalali(greg, &jalali);

    /* format as YYMMDD */
    sprintf(out_date, "%02d%02d%02d", jalali.year % 100, jalali.month, jalali.day);

    return 0;
}

static unsigned char intToCharTable[][2] = {
        {0, 			0x30},	//KEY1
		{1, 			0x31},	//KEY1
		{2, 			0x32},	//KEY2
		{3, 			0x33},	//KEY3
		{4, 			0x34},	//KEY4
		{5,		 		0x35},	//KEY5
		{6, 			0x36},	//KEY6
		{7, 			0x37},	//KEY7
		{8, 			0x38},	//KEY8
		{9, 			0x39}	//KEY9
};

unsigned char intToChar(int val)
{
	unsigned char ret = 0;
	if (val < 0) {
        return 0xFF;
	}
	for(uint16_t i = 0 ; i < sizeof(intToCharTable) / sizeof(intToCharTable[0]) ; i++) {
		if(val == intToCharTable[i][0]) {
			ret = intToCharTable[i][1];
		}
	}

	return ret;
}

inline void InsertDotIP(const char *ip, char* ip_add)
{
    /* Insert Dot between the octed of ip address of user prompt */
    char octed1[4], octed2[4], octed3[4], octed4[4];
    char TmpOcted1[4], TmpOcted2[4], TmpOcted3[4], TmpOcted4[4];
    memset(TmpOcted1, 0, sizeof(TmpOcted1));
    memset(TmpOcted2, 0, sizeof(TmpOcted2));
    memset(TmpOcted3, 0, sizeof(TmpOcted3));
    memset(TmpOcted4, 0, sizeof(TmpOcted4));
    memset(octed1, 0, sizeof(octed1));
    memset(octed2, 0, sizeof(octed2));
    memset(octed3, 0, sizeof(octed3));
    memset(octed4, 0, sizeof(octed4));
    memcpy(TmpOcted1, ip, 3);
    deleteZero(TmpOcted1,octed1);
    memcpy(TmpOcted2, ip + 3, 3);
    deleteZero(TmpOcted2,octed2);
    memcpy(TmpOcted3, ip + 6, 3);
    deleteZero(TmpOcted3,octed3);
    memcpy(TmpOcted4, ip + 9, 3);
    deleteZero(TmpOcted4,octed4);
    sprintf(ip_add, "%s.%s.%s.%s", octed1, octed2, octed3, octed4);
}

bool isDigit(char c)
{
    return (c >= '0' && c <= '9');
}

bool isAlphabetic(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int toInt(const char *value)
{
    return atoi(value);
}

int16_t hexCharToDecimal(char hexChar)
{
    switch (hexChar)
    {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'a':
        return 10;
    case 'A':
        return 10;
    case 'b':
        return 11;
    case 'B':
        return 11;
    case 'c':
        return 12;
    case 'C':
        return 12;
    case 'd':
        return 13;
    case 'D':
        return 13;
    case 'e':
        return 14;
    case 'E':
        return 14;
    case 'f':
        return 15;
    case 'F':
        return 15;
    default:
        return -1;
    }
}

int dateSeparator(const char *in, char *out, size_t out_size)
{
    size_t len;

    if (!in || !out)
        return -1;

    len = strlen(in);

    /* Accept only 6 or 8 digits */
    if (!(len == 6 || len == 8))
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)in[i]))
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
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        out[3] = in[3];
        out[4] = '/';
        out[5] = in[4];
        out[6] = in[5];
        out[7] = '/';
        out[8] = in[6];
        out[9] = in[7];
        out[10] = '\0';
    }

    return 0;
}

int timeSeparator(const char *in, char *out, size_t out_size)
{
    if (!in || !out)
        return -1;

    if (strlen(in) != 6)
        return -1;

    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)in[i]))
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

int appendChar(char *buf, size_t buf_size, char c)
{
    size_t len = 0;

    /* Find current length */
    while (len < buf_size && buf[len] != '\0')
        len++;

    /* No space left (need 1 byte for '\0') */
    if (len + 1 >= buf_size)
        return -1;

    buf[len] = c;
    buf[len + 1] = '\0';

    return 0;
}

int deleteChar(char *buf)
{
    size_t len = 0;

    while (buf[len] != '\0')
        len++;

    if (len == 0)
        return -1;   // nothing to delete

    buf[len - 1] = '\0';
    return 0;
}

void clearStr(char *buf)
{
    if (buf)
        buf[0] = '\0';
}

int amountSeparator(const char *in, char *out, size_t out_size)
{
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

    commas = (len - 1) / 3;
    out_len = len + commas;
    if (out_size < out_len + 1)
        return -1;
    
    out[out_len] = '\0';

    /* build output from right to left */
    i = len;
    j = out_len;
    int count = 0;

    while (i > 0) {
        out[--j] = in[--i];
        count++;

        if (count == 3 && i > 0) {
            out[--j] = ',';
            count = 0;
        }
    }

    return 0;
}

static const char *day_names[] = {
    "شنبه", "یکشنبه", "دوشنبه", "سه شنبه",
    "چهارشنبه", "پنج شنبه", "جمعه"
};

const char *getDayName(int y, int m, int d)
{
    if (m < 3) {
        m += 12;
        y -= 1;
    }

    int K = y % 100;
    int J = y / 100;

    int h = (d + (13 * (m + 1)) / 5 + K + (K / 4) + (J / 4) + 5 * J) % 7;
    return day_names[h];
}

int8_t getNameofDay(uint32_t date, char *out, size_t size) {
    RETURN_VALUE_IF_NULL(out, ;, ERR_BAD_PARAMETER);
    uint32_t y  = date / 10000;
    uint32_t m = (date % 10000) / 100;
    uint32_t d   = date % 100;
    if (m < 3) {
        m += 12;
        y -= 1;
    }

    int K = y % 100;
    int J = y / 100;

    int h = (d + (13 * (m + 1)) / 5 + K + (K / 4) + (J / 4) + 5 * J) % 7;
    snprintf(out, size, "s", day_names[h]);
    return ERR_OK;
}

Date_t getJalaliDate() {
    DateTime *dt = OOP_CALL(sys(), getDateTime);

    int yy, mm, dd;
    sscanf(dt->date, "%2d%2d%2d", &yy, &mm, &dd);

    int full_year = 2000 + yy;     // adjust if needed
    Date_t greg;
    greg.day = dd;
    greg.month = mm;
    greg.year = full_year;
    Date_t jalali;
    gregorianToJalali(greg, &jalali);
    return jalali;
}

void formatDateTimeStr(char *out, size_t out_size)
{
    DateTime *dt = OOP_CALL(sys(), getDateTime);

    int yy, mm, dd;
    sscanf(dt->date, "%2d%2d%2d", &yy, &mm, &dd);

    int full_year = 2000 + yy;     // adjust if needed
    const char *day = getDayName(full_year, mm, dd);
    Date_t greg;
    greg.day = dd;
    greg.month = mm;
    greg.year = full_year;
    Date_t jalali;
    gregorianToJalali(greg, &jalali);
    snprintf(out, out_size,
             "%02d/%02d/%02d-%s",
             jalali.year % 100, jalali.month, jalali.day, day);
}

void formatTimeStr(char *out, size_t out_size)
{
    DateTime *dt = OOP_CALL(sys(), getDateTime);

    int hh, mm;
    sscanf(dt->time, "%2d%2d", &hh, &mm);

    snprintf(out, out_size, "%02d:%02d", hh, mm);
}

void toPersianDigits(char *out, size_t out_size, int value)
{
    static const char *digits[] = {
        "۰","۱","۲","۳","۴","۵","۶","۷","۸","۹"
    };

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", value);

    out[0] = '\0';

    for (char *p = tmp; *p && strlen(out) + 4 < out_size; p++) {
        if (*p >= '0' && *p <= '9') {
            strcat(out, digits[*p - '0']);
        }
    }
}

void getDateTimeUint(uint32_t *date, uint32_t *time) {
    DateTime *dt = OOP_CALL(sys(), getDateTime);
    Date_t jd = getJalaliDate();
    int hh, mm, ss;
    sscanf(dt->time, "%2d%2d%2d", &hh, &mm, &ss);
    *date = jd.year * 10000 + 
            jd.month * 100 + jd.day;
    *time = hh * 10000 + 
            mm * 100 + ss;
}

void dateTimeToStr(uint32_t date, uint32_t time, char *str, size_t size) {
    char dt[24] = {0};
    int yy = date / 10000;
    int tmp = (date % 10000);
    int mm = tmp / 100;
    int dd = tmp % 100;

    int hh = time / 10000;
    tmp = (time % 10000);
    int min = tmp / 100;
    int ss = tmp % 100;
    snprintf(str, size, "%02d:%02d:%02d-%02d/%02d/%04d", ss, min, hh, dd, mm, yy);
}

void shortDateTimeToStr(uint32_t date, uint32_t time, char *str, size_t size) {
    int mm = (date % 10000) / 100;
    int dd = date % 100;

    int hh = time / 10000;
    int min = (time % 10000) / 100;

    snprintf(str, size, "%02d/%02d-%02d/%02d", mm, dd, hh, min);
}

void extractDatetimeStr(const char *buf, char *date, char *time) {
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

void extractDatetimeInt(const char *buf, uint32_t *date, uint32_t *time)
{
    char tmp[9];  // enough for "yyyymmdd" + '\0'

    if (strlen(buf) < 14) {
        *date = -1;
        *time = -1;
        return;
    }

    // Extract date (yyyymmdd)
    memcpy(tmp, buf, 8);
    tmp[8] = '\0';
    *date = toInt(tmp);

    // Extract time (hhmmss)
    memcpy(tmp, buf + 8, 6);
    tmp[6] = '\0';
    *time = toInt(tmp);
}

/*
 * Combines two uint32_t values into a single uint64_t
 * date: yyyymmdd (high 32 bits)
 * time: hhmmss   (low 32 bits)
 * returns: uint64_t combined as (date << 32) | time
 */
uint64_t packDateTime(uint32_t date, uint32_t time) {
    if (date > 99999999UL)   // 8-digit date
        return 0;           // or handle error

    if (time > 245959UL)     // max time you defined
        return 0;

    uint64_t dt = ((uint64_t)date << 18) | (uint64_t)time;
    return dt;
}


/*
 * Extract date and time from uint64_t
 * dateTime: packed value
 * date: output pointer to uint32_t (YYYMMDD)
 * time: output pointer to uint32_t (HHMMSS)
 */
void unpackDateTime(const  uint64_t *dt, uint32_t *date, uint32_t *time) {
    *time = (*dt) & ((1ULL << 18) - 1);
    *date = (*dt) >> 18;
}
