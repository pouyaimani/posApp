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
