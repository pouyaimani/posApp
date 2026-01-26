#include "utility.h"
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

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
    case 1: return "یک";
    case 2: return "دو";
    case 3: return "سه";
    case 4: return "چهار";
    case 5: return "پنج";
    case 6: return "شش";
    case 7: return "هفت";
    case 8: return "هشت";
    case 9: return "نه";
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

void gregorianToJalali(Calendar_t greg, Calendar_t * jalali)
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
    Calendar_t greg;
    greg.day = gd;
    greg.month = gm;
    greg.month = gy;
    Calendar_t jalali;
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