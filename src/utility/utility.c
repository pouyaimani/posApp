#include "utility.h"
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

void removeNonDigits(const char* src, char* dst, size_t dst_size) {
    size_t j = 0;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if (isDigit((unsigned char)src[i])) {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

void removeDots(const char* src, char* dst, size_t dst_size) {
    size_t j = 0;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if (src[i] != '.') {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

inline void InsertDotIP(const char* ip, char* ip_add) {
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
    deleteZero(TmpOcted1, octed1);
    memcpy(TmpOcted2, ip + 3, 3);
    deleteZero(TmpOcted2, octed2);
    memcpy(TmpOcted3, ip + 6, 3);
    deleteZero(TmpOcted3, octed3);
    memcpy(TmpOcted4, ip + 9, 3);
    deleteZero(TmpOcted4, octed4);
    sprintf(ip_add, "%s.%s.%s.%s", octed1, octed2, octed3, octed4);
}

bool isDigit(char c) { return (c >= '0' && c <= '9'); }

bool isAlphabetic(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int appendChar(char* buf, size_t buf_size, char c) {
    size_t len = 0;

    /* Find current length */
    while (len < buf_size && buf[len] != '\0')
        len++;

    /* No space left (need 1 byte for '\0') */
    if (len + 1 >= buf_size)
        return -1;

    buf[len]     = c;
    buf[len + 1] = '\0';

    return 0;
}

int deleteChar(char* buf) {
    size_t len = 0;

    while (buf[len] != '\0')
        len++;

    if (len == 0)
        return -1; // nothing to delete

    buf[len - 1] = '\0';
    return 0;
}

void clearStr(char* buf) {
    if (buf)
        buf[0] = '\0';
}

static const char* day_names[] = {"شنبه",     "یکشنبه",   "دوشنبه", "سه شنبه",
                                  "چهارشنبه", "پنج شنبه", "جمعه"};

const char* getDayName(int y, int m, int d) {
    if (m < 3) {
        m += 12;
        y -= 1;
    }

    int K = y % 100;
    int J = y / 100;

    int h = (d + (13 * (m + 1)) / 5 + K + (K / 4) + (J / 4) + 5 * J) % 7;
    return day_names[h];
}

int8_t getNameofDay(uint32_t date, char* out, size_t size) {
    RETURN_VALUE_IF_NULL(out, ;, ERR_BAD_PARAMETER);
    uint32_t y = date / 10000;
    uint32_t m = (date % 10000) / 100;
    uint32_t d = date % 100;
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
/*
 * Combines two uint32_t values into a single uint64_t
 * date: yyyymmdd (high 32 bits)
 * time: hhmmss   (low 32 bits)
 * returns: uint64_t combined as (date << 32) | time
 */
uint64_t packDateTime(uint32_t date, uint32_t time) {
    if (date > 99999999UL) // 8-digit date
        return 0;          // or handle error

    if (time > 245959UL) // max time you defined
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
void unpackDateTime(const uint64_t* dt, uint32_t* date, uint32_t* time) {
    *time = (*dt) & ((1ULL << 18) - 1);
    *date = (*dt) >> 18;
}

void leftPadZero(const char* src, char* dst, size_t dstSize, int width) {
    snprintf(dst, dstSize, "%0*s", width, src);
}

void prependZerosInt(uint64_t number, int totalWidth, char* output,
                     size_t outputSize) {
    snprintf(output, outputSize, "%0*d", totalWidth, number);
}

void prependZerosUInt64(uint64_t number, int totalWidth, char* output,
                        size_t outputSize) {
    snprintf(output, outputSize, "%0*" PRIu64, totalWidth, number);
}

bool ipFormatLeftAligned(const char* in, char* out) {
    char digits[12] = {0};
    int  dcount     = 0;

    // Extract digits only (max 12)
    for (int i = 0; in[i] && dcount < 12; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[dcount++] = in[i];
        }
    }

    int  pos   = 0;
    int  di    = 0;
    bool valid = true;

    for (int oct = 0; oct < 4; oct++) {

        // If not enough digits for this octet → fill with "---"
        if (di >= dcount) {
            out[pos++] = '-';
            out[pos++] = '-';
            out[pos++] = '-';
        } else {
            int octVal = 0;
            int start  = di;

            // Read up to 3 digits
            int len = 0;
            while (di < dcount && len < 3) {
                octVal = octVal * 10 + (digits[di] - '0');
                di++;
                len++;
            }

            // Clamp to 255 if needed
            if (octVal > 255) {
                octVal = 255;
                valid  = false;
            }

            // Write as 3-digit zero-padded number
            pos += sprintf(out + pos, "%03d", octVal);
        }

        if (oct < 3) {
            out[pos++] = '.';
        }
    }

    out[pos] = '\0';
    return valid;
}

bool normalizeIp(const char* input, char* output, size_t outputSize) {
    if (input == NULL || output == NULL)
        return false;

    // Must be exactly 12 digits
    if (strlen(input) != 12)
        return false;

    for (int i = 0; i < 12; i++) {
        if (!isDigit((unsigned char)input[i]))
            return false;
    }

    char octetStr[4];
    int  octet[4];

    for (int i = 0; i < 4; i++) {
        memcpy(octetStr, input + (i * 3), 3);
        octetStr[3] = '\0';

        octet[i] = libAtoi(octetStr);
        // Validate IPv4 range
        if (octet[i] < 0 || octet[i] > 255)
            return false;
    }
    // Build normalized IP
    snprintf(output, outputSize, "%d.%d.%d.%d", octet[0], octet[1], octet[2],
             octet[3]);

    return true;
}

bool dateValidate(const char* in) {
    char digits[8];
    int  len = 0;

    for (int i = 0; in[i] && len < 8; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[len++] = in[i];
        }
    }

    if (len != 8)
        return false;

    int year = 0, month = 0, day = 0;

    // YYYY
    for (int i = 0; i < 4; i++)
        year = year * 10 + (digits[i] - '0');

    // MM
    for (int i = 4; i < 6; i++)
        month = month * 10 + (digits[i] - '0');

    // DD
    for (int i = 6; i < 8; i++)
        day = day * 10 + (digits[i] - '0');

    if (month < 1 || month > 12)
        return false;
    if (day < 1 || day > 31)
        return false;

    // Optional: better day validation per month
    int maxDay = 31;
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        maxDay    = leap ? 29 : 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        maxDay = 30;
    }

    return day <= maxDay;
}

bool timeValidate(const char* in) {
    char digits[6];
    int  len = 0;

    for (int i = 0; in[i] && len < 6; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[len++] = in[i];
        }
    }

    if (len != 6)
        return false;

    int hh = (digits[0] - '0') * 10 + (digits[1] - '0');
    int mm = (digits[2] - '0') * 10 + (digits[3] - '0');
    int ss = (digits[4] - '0') * 10 + (digits[5] - '0');

    if (hh > 23)
        return false;
    if (mm > 59)
        return false;
    if (ss > 59)
        return false;

    return true;
}

char* skipLeadingZeros(char* str) {
    if (str == NULL)
        return NULL;

    while (*str == '0' && *(str + 1) != '\0')
        str++;

    return str;
}

bool removeLeadingZeros(const char* input, char* output, size_t outputSize) {
    if (input == NULL || output == NULL || outputSize == 0)
        return false;

    while (*input == '0' && *(input + 1) != '\0')
        input++;

    size_t len = strlen(input);

    if (len + 1 > outputSize)
        return false;

    memcpy(output, input, len + 1);

    return true;
}

bool extractBin(const char* pan, char* bin, size_t binSize, size_t binLen) {
    if (pan == NULL || bin == NULL)
        return false;

    if (binLen != 6 && binLen != 8)
        return false;

    size_t panLen = strlen(pan);

    if (panLen < binLen)
        return false;

    if (binSize < (binLen + 1))
        return false;

    memcpy(bin, pan, binLen);
    bin[binLen] = '\0';

    return true;
}

int maskPan(const char* pan, char* masked, size_t masked_size) {
    if (pan == NULL || masked == NULL)
        return -1;

    size_t len = strlen(pan);

    /* PAN must contain at least 10 digits:
       first 6 + last 4 */
    if (len < 10)
        return -2;

    /* Ensure output buffer is large enough */
    if (masked_size < len + 1)
        return -3;

    strcpy(masked, pan);

    /* Replace everything between first 6 and last 4 */
    for (size_t i = 6; i < len - 4; i++)
        masked[i] = '*';

    return 0;
}

void padRight(const char* unpadded, int unpadlength, int len, char* padded,
              char p) {
    int j = 0, i;
    for (i = 0; i < unpadlength; i++) {
        padded[i] = unpadded[j++];
    }
    for (i = unpadlength; i < len; i++) {
        padded[i] = p;
    }
    padded[len] = '\0';
}

void padLeft(const char* unpadded, int unpadlength, int len, char* padded,
             char p) {
    int j = 0, i;

    for (i = 0; i < (len - unpadlength); i++)
        padded[i] = p;
    for (i = (len - unpadlength); i < len; i++) {
        padded[i] = unpadded[j++];
    }
    padded[len] = '\0';
}

int getBillType(const char* billId, size_t len) {
    char digit = billId[len - 2];
    return libAtoi(&digit);
}

const char* getBillOrgName(BillType_t id) {
    Phrases_t phrase = PHRASE_BILL_INVALID;
    switch (id) {
    //  Public Services | Pasargad Life Insurance
    case BILL_PUBLIC_SERVICE:
        phrase = PHRASE_PUBLIC_SERVICE;
        break;
    // Water
    case BILL_WATER:
        phrase = PHRASE_BILL_WATER;
        break;
    // Electricity
    case BILL_ELECTRICITY:
        phrase = PHRASE_BILL_ELECTRICITY;
        break;
    // Gas
    case BILL_GAS:
        phrase = PHRASE_BILL_GAS;
        break;
    // Telephone
    case BILL_TELEPHONE:
        phrase = PHRASE_BILL_TELEPHONE;
        break;
    // Mobile
    case BILL_MOBILE:
        phrase = PHRASE_BILL_MOBILE;
        break;
    // Municipality
    case BILL_MUNICIPALIT:
    case 7:
        phrase = PHRASE_BILL_MUNICIPALITY;
        break;
    // Tax
    case BILL_TAX:
        phrase = PHRASE_BILL_TAX;
        break;
    // Driving Policy
    case BILL_DRIVE_POLIC:
        phrase = PHRASE_BILL_DRIVE_POLICY;
        break;

    default:
        break;
    }

    return phraseGetDef(phrase);
}

static int calculateMod11(const char* digits) {
    int    sum = 0;
    size_t len = strlen(digits);

    for (int i = (int)len - 1, weight = 2; i >= 0; --i) {
        sum += (digits[i] - '0') * weight;
        weight = (weight == 7) ? 2 : weight + 1;
    }

    int digit = 11 - (sum % 11);

    return (digit > 9) ? 0 : digit;
}

bool billExtractAmount(const char* paymentId, char* amount, size_t alen) {
    size_t len = strlen(paymentId);
    // TODO: check size of amount
    memset(amount, 0, alen);
    memcpy(amount, paymentId, len - 5);
    strcat(amount, "000");
    return true;
}

bool isBillIdValid(const char* billId) {
    size_t inputlen = strlen(billId);
    if (inputlen < LEN_BILL_ID_MIN || inputlen > LEN_BILL_ID_MAX) {
        return false;
    }

    DEFINE_STRING(paddedBillId, LEN_BILL_ID_MAX);
    padLeft(billId, inputlen, LEN_BILL_ID_MAX, paddedBillId, '0');

    const size_t len = strlen(paddedBillId);

    int sum = 0;

    for (int i = (int)len - 2, weight = 2; i >= 0; --i) {
        sum += (paddedBillId[i] - '0') * weight;
        weight = (weight == 7) ? 2 : weight + 1;
    }

    int checkDigit = 11 - (sum % 11);
    checkDigit     = (checkDigit > 9) ? 0 : checkDigit;

    return checkDigit != (paddedBillId[len - 1] - '0');
}

bool isBillValid(const char* billId, const char* paymentId) {
    size_t len = strlen(paymentId);

    if (len < 6 || len > 13) {
        return false;
    }
    bool res = calculateMod11(paymentId) == (paymentId[len - 1] - '0');
    RETURN_VALUE_IF_NOT(res, true, ;, false);
    DEFINE_STRING(tmp, (LEN_PAYMENT_ID_MAX * 2 + 1));
    strcpy(tmp, billId);
    strcat(tmp, paymentId);
    len = strlen(tmp);
    return calculateMod11(tmp) == (tmp[len - 1] - '0');
}