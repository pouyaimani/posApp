#include "convert.h"
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
#include "error.h"

int libAtoi(const char* str) {
    int  s    = 0;
    bool falg = false;

    while (*str == ' ') {
        str++;
    }
    if (*str == '-' || *str == '+') {
        if (*str == '-')
            falg = true;
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        s = s * 10 + *str - '0';
        str++;
        if (s < 0) {
            s = 2147483647;
            break;
        }
    }
    return s * (falg ? -1 : 1);
}

bool str2u64(const char* str, uint64_t* value) {
    uint64_t result = 0;

    if (str == NULL || value == NULL)
        return false;

    if (*str == '\0')
        return false;

    while (*str) {
        if (*str < '0' || *str > '9')
            return false;

        uint64_t digit = (uint64_t)(*str - '0');

        /* Overflow check */
        if (result > (UINT64_MAX - digit) / 10)
            return false;

        result = result * 10 + digit;
        ++str;
    }

    *value = result;
    return true;
}

bool u64toStr(uint64_t value, char* str, size_t size) {
    char   tmp[20];
    size_t len = 0;

    if (str == NULL || size == 0)
        return false;

    if (value == 0) {
        if (size < 2)
            return false;

        str[0] = '0';
        str[1] = '\0';
        return true;
    }

    while (value) {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    if (len + 1 > size)
        return false;

    for (size_t i = 0; i < len; ++i)
        str[i] = tmp[len - i - 1];

    str[len] = '\0';
    return true;
}

int intToStr(int val, char* out, size_t size) {
    memset(out, 0, size);
    return sprintf(out, "%d", val);
}

bool stringToNumber(const char* str, void* out, NumberType type) {
    bool     negative = false;
    uint64_t value    = 0;

    RETURN_VALUE_IF_NULL(str, ;, false);
    RETURN_VALUE_IF_NULL(out, ;, false);

    /* Handle optional sign */
    if (*str == '-') {
        negative = true;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Empty string after sign */
    RETURN_VALUE_IF((*str), '\0', ;, false);

    /* Parse digits */
    while (*str != '\0') {
        if ((*str < '0') || (*str > '9'))
            return false;

        uint32_t digit = (uint32_t)(*str - '0');

        /* Detect uint64 overflow */
        RETURN_VALUE_IF_GREATER(value, ((UINT64_MAX - digit) / 10), ;, false);
        value = (value * 10U) + digit;

        str++;
    }

    switch (type) {
    case TYPE_INT8: {
        if (negative) {
            RETURN_VALUE_IF_GREATER(value, ((uint64_t)INT8_MAX + 1), ;, false);

            *(int8_t*)out = (int8_t)(-(int64_t)value);
        } else {
            RETURN_VALUE_IF_GREATER(value, INT8_MAX, ;, false);
            *(int8_t*)out = (int8_t)value;
        }

        break;
    }

    case TYPE_UINT8: {
        RETURN_VALUE_IF_GREATER(value, UINT8_MAX, ;, false);
        RETURN_VALUE_IF(negative, true, ;, false);

        *(uint8_t*)out = (uint8_t)value;
        break;
    }

    case TYPE_INT16: {
        if (negative) {
            RETURN_VALUE_IF_GREATER(value, ((uint64_t)INT16_MAX + 1), ;, false);

            *(int16_t*)out = (int16_t)(-(int64_t)value);
        } else {
            RETURN_VALUE_IF_GREATER(value, INT16_MAX, ;, false);

            *(int16_t*)out = (int16_t)value;
        }

        break;
    }

    case TYPE_UINT16: {
        RETURN_VALUE_IF_GREATER(value, UINT16_MAX, ;, false);
        RETURN_VALUE_IF(negative, true, ;, false);

        *(uint16_t*)out = (uint16_t)value;
        break;
    }

    case TYPE_INT32: {
        if (negative) {
            RETURN_VALUE_IF_GREATER(value, ((uint64_t)INT32_MAX + 1), ;, false);

            *(int32_t*)out = (int32_t)(-(int64_t)value);
        } else {
            RETURN_VALUE_IF_GREATER(value, (INT32_MAX), ;, false);

            *(int32_t*)out = (int32_t)value;
        }

        break;
    }

    case TYPE_UINT32: {
        RETURN_VALUE_IF_GREATER(value, UINT32_MAX, ;, false);
        RETURN_VALUE_IF(negative, true, ;, false);

        *(uint32_t*)out = (uint32_t)value;
        break;
    }

    case TYPE_INT64: {
        if (negative) {
            RETURN_VALUE_IF_GREATER(value, ((uint64_t)INT64_MAX + 1ULL), ;
                                    , false);

            *(int64_t*)out = -(int64_t)value;
        } else {
            RETURN_VALUE_IF_GREATER(value, INT64_MAX, ;, false);

            *(int64_t*)out = (int64_t)value;
        }

        break;
    }

    case TYPE_UINT64: {
        RETURN_VALUE_IF(negative, true, ;, false);

        *(uint64_t*)out = value;
        break;
    }

    default:
        return false;
    }

    return true;
}

static void reverse(char* str, uint32_t len) {
    uint32_t i = 0;
    uint32_t j = len - 1;

    while (i < j) {
        char tmp = str[i];
        str[i]   = str[j];
        str[j]   = tmp;

        i++;
        j--;
    }
}

bool numberToString(const void* value, NumberType type, char* buffer,
                    uint32_t size) {
    uint64_t u64      = 0;
    bool     negative = false;
    uint32_t pos      = 0;

    if ((value == NULL) || (buffer == NULL) || (size < 2))
        return false;

    switch (type) {
    case TYPE_INT8: {
        int8_t v = *(const int8_t*)value;
        negative = (v < 0);
        u64      = negative ? -(int64_t)v : v;
        break;
    }

    case TYPE_UINT8:
        u64 = *(const uint8_t*)value;
        break;

    case TYPE_INT16: {
        int16_t v = *(const int16_t*)value;
        negative  = (v < 0);
        u64       = negative ? -(int64_t)v : v;
        break;
    }

    case TYPE_UINT16:
        u64 = *(const uint16_t*)value;
        break;

    case TYPE_INT32: {
        int32_t v = *(const int32_t*)value;
        negative  = (v < 0);
        u64       = negative ? -(int64_t)v : v;
        break;
    }

    case TYPE_UINT32:
        u64 = *(const uint32_t*)value;
        break;

    case TYPE_INT64: {
        int64_t v = *(const int64_t*)value;
        negative  = (v < 0);

        /*
         * Handles INT64_MIN correctly.
         */
        if (negative)
            u64 = (uint64_t)(-(v + 1)) + 1;
        else
            u64 = (uint64_t)v;

        break;
    }

    case TYPE_UINT64:
        u64 = *(const uint64_t*)value;
        break;

    default:
        return false;
    }

    /* Special case for zero */
    if (u64 == 0) {
        if (size < 2)
            return false;

        buffer[0] = '0';
        buffer[1] = '\0';

        return true;
    }

    /* Generate digits in reverse order */
    while (u64) {
        if (pos >= (size - 1))
            return false;

        buffer[pos++] = '0' + (u64 % 10);
        u64 /= 10;
    }

    if (negative) {
        if (pos >= (size - 1))
            return false;

        buffer[pos++] = '-';
    }

    buffer[pos] = '\0';

    reverse(buffer, pos);

    return true;
}

int toInt(const char* value) { return libAtoi(value); }

static unsigned char intToCharTable[][2] = {
    {0, 0x30}, // KEY1
    {1, 0x31}, // KEY1
    {2, 0x32}, // KEY2
    {3, 0x33}, // KEY3
    {4, 0x34}, // KEY4
    {5, 0x35}, // KEY5
    {6, 0x36}, // KEY6
    {7, 0x37}, // KEY7
    {8, 0x38}, // KEY8
    {9, 0x39}  // KEY9
};

unsigned char intToChar(int val) {
    unsigned char ret = 0;
    if (val < 0) {
        return 0xFF;
    }
    for (uint16_t i = 0; i < sizeof(intToCharTable) / sizeof(intToCharTable[0]);
         i++) {
        if (val == intToCharTable[i][0]) {
            ret = intToCharTable[i][1];
        }
    }

    return ret;
}

typedef struct {
    char*  buf;
    size_t size;
    size_t len;
} writer_t;

static const char* const yekan_words[] = {
    "",       "یک",     "دو",     "سه",   "چهار",  "پنج",    "شش",
    "هفت",    "هشت",    "نه",     "ده",   "یازده", "دوازده", "سیزده",
    "چهارده", "پانزده", "شانزده", "هفده", "هجده",  "نوزده"};

static const char* const dahgan_words[] = {
    "", "", "بیست", "سی", "چهل", "پنجاه", "شصت", "هفتاد", "هشتاد", "نود"};

static const char* const sadgan_words[] = {"",       "صد",    "دویست", "سیصد",
                                           "چهارصد", "پانصد", "ششصد",  "هفتصد",
                                           "هشتصد",  "نهصد"};

static const char* const scale_words[] = {"", "هزار", "میلیون", "میلیارد",
                                          "تریلیون"};

static void w_append(writer_t* w, const char* s) {
    while (*s && w->len + 1 < w->size) {
        w->buf[w->len++] = *s++;
    }
    w->buf[w->len] = '\0';
}

static void convert_3digits(writer_t* w, int n) {
    int h = n / 100;
    int r = n % 100;

    if (h) {
        w_append(w, sadgan_words[h]);
        if (r)
            w_append(w, " و ");
    }

    if (r < 20) {
        if (r)
            w_append(w, yekan_words[r]);
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

int addBeHarf(const char* num, char* out, size_t out_size) {
    writer_t w           = {out, out_size, 0};
    int      groups[5]   = {0}; /* up to trillions */
    int      group_count = 0;
    int      negative    = 0;

    if (!num || !out || out_size == 0)
        return -1;

    if (*num == '-') {
        negative = 1;
        num++;
    }

    /* validate */
    for (const char* p = num; *p; p++) {
        if (!isDigit((unsigned char)*p))
            return -1;
    }

    /* split into 3-digit groups */
    int len    = strlen(num);
    int factor = 1;
    int acc    = 0;

    for (int i = len - 1; i >= 0; i--) {
        acc += (num[i] - '0') * factor;
        factor *= 10;

        if (factor == 1000 || i == 0) {
            groups[group_count++] = acc;
            acc                   = 0;
            factor                = 1;
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

    if (first) /* number was zero */
        w_append(&w, "صفر");

    return 0;
}

void toPersianDigits(char* out, size_t out_size, int value) {
    static const char* digits[] = {"۰", "۱", "۲", "۳", "۴",
                                   "۵", "۶", "۷", "۸", "۹"};

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", value);

    out[0] = '\0';

    for (char* p = tmp; *p && strlen(out) + 4 < out_size; p++) {
        if (*p >= '0' && *p <= '9') {
            strcat(out, digits[*p - '0']);
        }
    }
}

int hex2data(unsigned char* data, const unsigned char* hexstring,
             unsigned int len) {
    unsigned const char* pos = hexstring;
    char*                endptr;
    int                  count = 0;

    if ((hexstring[0] == '\0') || (strlen((char*)hexstring) % 2)) {
        // hexstring contains no data or hexstring has an odd length
        return -1;
    }

    for (count = 0; count < len; count++) {
        char buf[5];
        memset(buf, 0x00, sizeof(buf));
        buf[0]      = '0';
        buf[1]      = 'x';
        buf[2]      = pos[0];
        buf[3]      = pos[1];
        data[count] = strtol(buf, &endptr, 0);
        pos += 2 * sizeof(char);

        if (endptr[0] != '\0') {
            // non-hexadecimal character encountered
            return -1;
        }
    }

    return 0;
}

static unsigned char nibbleFromChar(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 255;
}

void hexStringToBytes(char* inhex, int len, unsigned char* retval) {
    unsigned char* p;
    int            i;

    p = (unsigned char*)inhex;
    for (i = 0; i < len; i++) {
        retval[i] = (nibbleFromChar(*p) << 4) | nibbleFromChar(*(p + 1));
        p += 2;
    }
}

void convertStrNoNumber(char* source, int len, unsigned char hex,
                        int startCompare, int endCompare, char* dest) {
    char winChar[52][5] = {
        " ", // Space
        " ", // Space
        "0", // 0
        "1", // 1
        "2", // 2
        "3", // 3
        "4", // 4
        "5", // 5
        "6", // 6
        "7", // 7
        "8", // 8
        "9", // 9
        "،", // Comma
        "~", // Stretch
        "؟", // Question
        "آ", // Alef (Madda)
        "ئ", // Yeh (Hamza)
        "ء", // Hamza
        "ا", // Alef
        "ب", // Beh
        "پ", // Peh
        "ت", // Teh
        "ث", // Theh
        "ج", // Jeem
        "چ", // Tcheh
        "ح", // Hah
        "خ", // Khah
        "د", // Dal
        "ذ", // Thal
        "ر", // Reh
        "ز", // Zein
        "ژ", // Jeh
        "س", // Seen
        "ش", // Sheen
        "ص", // Sad
        "ض", // Dad
        "ط", // Tah
        "ظ", // Zah
        "ع", // Ain
        "غ", // Ghain
        "ف", // Feh
        "ق", // Qaf
        "ک", // Kaf
        "ک", // Kaf
        "گ", // Gaf
        "ل", // Lam
        "م", // Meem
        "ن", // Noon
        "و", // Waw
        "ه", // Heh
        "ی", // Yeh
        "ی"  // Yeh
    };
    int codePage[52][7] = {
        // coherency (0x00 : none, 0x01 : before, 0x02 : after, 0x03 : both
        // 4 iran system		         ,arabic,unicode,coherency
        {0x20, 0x20, 0x20, 0x20, 0x20, 0x0020, 0x00}, // Space
        {0xFF, 0xFF, 0xFF, 0xFF, 0x2D, 0x0020, 0x00}, // Space
        {0x80, 0x80, 0x80, 0x80, 0x30, 0x0030, 0x00}, // 0
        {0x81, 0x81, 0x81, 0x81, 0x31, 0x0031, 0x00}, // 1
        {0x82, 0x82, 0x82, 0x82, 0x32, 0x0032, 0x00}, // 2
        {0x83, 0x83, 0x83, 0x83, 0x33, 0x0033, 0x00}, // 3
        {0x84, 0x84, 0x84, 0x84, 0x34, 0x0034, 0x00}, // 4
        {0x85, 0x85, 0x85, 0x85, 0x35, 0x0035, 0x00}, // 5
        {0x86, 0x86, 0x86, 0x86, 0x36, 0x0036, 0x00}, // 6
        {0x87, 0x87, 0x87, 0x87, 0x37, 0x0037, 0x00}, // 7
        {0x88, 0x88, 0x88, 0x88, 0x38, 0x0038, 0x00}, // 8
        {0x89, 0x89, 0x89, 0x89, 0x39, 0x0039, 0x00}, // 9
        {0x8A, 0x8A, 0x8A, 0x8A, 0xA1, 0x060C, 0x00}, // Comma
        {0x8B, 0x8B, 0x8B, 0x8B, 0xDC, 0x0640, 0x02}, // Stretch
        {0x8C, 0x8C, 0x8C, 0x8C, 0x3F, 0x061F, 0x00}, // Question
        {0x8D, 0x8D, 0x8D, 0x8D, 0xC2, 0x0622, 0x01}, // Alef (Madda)
        {0x8E, 0x8E, 0x8E, 0x8E, 0xC6, 0x0626, 0x02}, // Yeh (Hamza)
        {0x8F, 0x8F, 0x8F, 0x8F, 0xC1, 0x0621, 0x00}, // Hamza
        {0x90, 0x90, 0x91, 0x91, 0xC7, 0x0627, 0x01}, // Alef
        {0x92, 0x93, 0x92, 0x93, 0xC8, 0x0628, 0x02}, // Beh
        {0x94, 0x95, 0x94, 0x95, 0x81, 0x067E, 0x02}, // Peh
        {0x96, 0x97, 0x96, 0x97, 0xCA, 0x062A, 0x02}, // Teh
        {0x98, 0x99, 0x98, 0x99, 0xCB, 0x062B, 0x02}, // Theh
        {0x9A, 0x9B, 0x9A, 0x9B, 0xCC, 0x062C, 0x02}, // Jeem
        {0x9C, 0x9D, 0x9C, 0x9D, 0x8D, 0x0686, 0x02}, // Tcheh
        {0x9E, 0x9F, 0x9E, 0x9F, 0xCD, 0x062D, 0x02}, // Hah
        {0xA0, 0xA1, 0xA0, 0xA1, 0xCE, 0x062E, 0x02}, // Khah
        {0xA2, 0xA2, 0xA2, 0xA2, 0xCF, 0x062F, 0x01}, // Dal
        {0xA3, 0xA3, 0xA3, 0xA3, 0xD0, 0x0630, 0x01}, // Thal
        {0xA4, 0xA4, 0xA4, 0xA4, 0xD1, 0x0631, 0x01}, // Reh
        {0xA5, 0xA5, 0xA5, 0xA5, 0xD2, 0x0632, 0x01}, // Zein
        {0xA6, 0xA6, 0xA6, 0xA6, 0x8E, 0x0698, 0x01}, // Jeh
        {0xA7, 0xA8, 0xA7, 0xA8, 0xD3, 0x0633, 0x02}, // Seen
        {0xA9, 0xAA, 0xA9, 0xAA, 0xD4, 0x0634, 0x02}, // Sheen
        {0xAB, 0xAC, 0xAB, 0xAC, 0xD5, 0x0635, 0x02}, // Sad
        {0xAD, 0xAE, 0xAD, 0xAE, 0xD6, 0x0636, 0x02}, // Dad
        {0xAF, 0xAF, 0xAF, 0xAF, 0xD8, 0x0637, 0x03}, // Tah
        {0xE0, 0xE0, 0xE0, 0xE0, 0xD9, 0x0638, 0x03}, // Zah
        {0xE1, 0xE4, 0xE2, 0xE3, 0xDA, 0x0639, 0x03}, // Ain
        {0xE5, 0xE8, 0xE6, 0xE7, 0xDB, 0x063A, 0x03}, // Ghain
        {0xE9, 0xEA, 0xE9, 0xEA, 0xDD, 0x0641, 0x02}, // Feh
        {0xEB, 0xEC, 0xEB, 0xEC, 0xDE, 0x0642, 0x02}, // Qaf
        {0xED, 0xEE, 0xED, 0xEE, 0xDF, 0x06A9, 0x02}, // Kaf <===
        {0xED, 0xEE, 0xED, 0xEE, 0x98, 0x06A9, 0x02}, // Kaf <===
        {0xEF, 0xF0, 0xEF, 0xF0, 0x90, 0x06AF, 0x02}, // Gaf
        {0xF1, 0xF3, 0xF1, 0xF3, 0xE1, 0x0644, 0x02}, // Lam
        {0xF4, 0xF5, 0xF4, 0xF5, 0xE3, 0x0645, 0x02}, // Meem
        {0xF6, 0xF7, 0xF6, 0xF7, 0xE4, 0x0646, 0x02}, // Noon
        {0xF8, 0xF8, 0xF8, 0xF8, 0xE6, 0x0648, 0x01}, // Waw
        {0xF9, 0xFB, 0xF9, 0xFA, 0xE5, 0x0647, 0x03}, // Heh
        {0xFD, 0xFE, 0xFC, 0xFE, 0xEC, 0x06CC, 0x03}, // Yeh <===
        {0xFD, 0xFE, 0xFC, 0xFE, 0xED, 0x06CC, 0x03}  // Yeh <===
    };
    unsigned char input[256];
    // int iMax = strlen(source);
    int i;

    memset(input, 0x00, sizeof(input));
    if (hex == 0)
        memcpy(input, source, len);
    // else
    // ToDo : pubHexStringToBytes(source, len, input);

    for (i = 0; i < len; i++) {
        int c      = input[i];
        int bFound = 0;
        int j      = 0;
        int jMax   = 52;
        for (j = 0; j < jMax; j++) {
            int k;
            for (k = startCompare; k < endCompare; k++) {
                if (c == codePage[j][k]) {
                    strcat(dest, winChar[j]);
                    bFound = 1;
                    break;
                }
            } // for (int k = 4; k < 5; k++)

            if (bFound)
                break;
        }
    } // for (int i = 0; i < iMax; i++)
}

void bytesToHex(const uint8_t* data, size_t dataLen, char* out,
                size_t outSize) {
    static const char hex[] = "0123456789ABCDEF";

    if (!data || !out || outSize < (dataLen * 2 + 1))
        return;

    for (size_t i = 0; i < dataLen; i++) {
        out[i * 2]     = hex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[data[i] & 0x0F];
    }

    out[dataLen * 2] = '\0';
}

int16_t hexCharToDecimal(char hexChar) {
    switch (hexChar) {
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

const char* numToLiteral(int x) {
    switch (x) {
    case 1:
        return phraseGetDef(PHRASE_ONE);
    case 2:
        return phraseGetDef(PHRASE_TWO);
    case 3:
        return phraseGetDef(PHRASE_THREE);
    case 4:
        return phraseGetDef(PHRASE_FOUR);
    case 5:
        return phraseGetDef(PHRASE_FIVE);
    case 6:
        return phraseGetDef(PHRASE_SIX);
    case 7:
        return phraseGetDef(PHRASE_SEVEN);
    case 8:
        return phraseGetDef(PHRASE_EIGHT);
    case 9:
        return phraseGetDef(PHRASE_NINE);
    default:
        return "";
    }
}