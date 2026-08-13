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
#include "settings/settings.h"
#include "network/network.h"

/**
 * @brief Copy only numeric characters from source string.
 *
 * Extracts all digits from @p src and writes them to @p dst.
 * The output is always null-terminated.
 *
 * @param src Input string.
 * @param dst Output buffer.
 * @param dst_size Size of output buffer.
 */
void removeNonDigits(const char* src, char* dst, size_t dst_size) {
    size_t j = 0;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if (isDigit((unsigned char)src[i])) {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

/**
 * @brief Remove all '.' characters from a string.
 *
 * Copies all characters except dots into the destination buffer.
 *
 * @param src Input string.
 * @param dst Output buffer.
 * @param dst_size Size of output buffer.
 */
void removeDots(const char* src, char* dst, size_t dst_size) {
    size_t j = 0;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if (src[i] != '.') {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

/**
 * @brief Convert a 12-digit IP representation to dotted format.
 *
 * Example:
 * "192168001001" -> "192.168.1.1"
 *
 * Leading zeros are removed from each octet.
 *
 * @param ip Input 12-digit IP string.
 * @param ip_add Output buffer for formatted IP address.
 */

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

/**
 * @brief Check whether a character is a decimal digit.
 *
 * @param c Character to test.
 * @return true if character is '0'..'9'.
 */
bool isDigit(char c) { return (c >= '0' && c <= '9'); }

/**
 * @brief Check whether a character is alphabetic.
 *
 * @param c Character to test.
 * @return true if character is A-Z or a-z.
 */
bool isAlphabetic(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/**
 * @brief Append a character to the end of a string.
 *
 * @param buf Null-terminated string buffer.
 * @param buf_size Total size of buffer.
 * @param c Character to append.
 *
 * @return 0 on success, -1 if buffer is full.
 */
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

/**
 * @brief Remove the last character from a string.
 *
 * @param buf Null-terminated string.
 *
 * @return 0 on success, -1 if string is empty.
 */
int deleteChar(char* buf) {
    size_t len = 0;

    while (buf[len] != '\0')
        len++;

    if (len == 0)
        return -1; // nothing to delete

    buf[len - 1] = '\0';
    return 0;
}

/**
 * @brief Clear a string.
 *
 * Sets the first character to '\0'.
 *
 * @param buf String buffer.
 */
void clearStr(char* buf) {
    if (buf)
        buf[0] = '\0';
}

static const char* day_names[] = {"شنبه",     "یکشنبه",   "دوشنبه", "سه شنبه",
                                  "چهارشنبه", "پنج شنبه", "جمعه"};

/**
 * @brief Calculate Persian day name for a given date.
 *
 * Uses Zeller's congruence to determine weekday.
 *
 * @param y Year.
 * @param m Month.
 * @param d Day.
 *
 * @return Pointer to Persian day name string.
 */
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

/**
 * @brief Convert YYYYMMDD date into Persian day name.
 *
 * @param date Date in YYYYMMDD format.
 * @param out Output buffer.
 * @param size Output buffer size.
 *
 * @return ERR_OK on success, error code otherwise.
 */
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

/**
 * @brief Pack date and time into a 64-bit value.
 *
 * Date occupies upper bits and time occupies lower 18 bits.
 *
 * @param date Date in YYYYMMDD format.
 * @param time Time in HHMMSS format.
 *
 * @return Packed value, or 0 if input is invalid.
 */
uint64_t packDateTime(uint32_t date, uint32_t time) {
    if (date > 99999999UL) // 8-digit date
        return 0;          // or handle error

    if (time > 245959UL) // max time you defined
        return 0;

    uint64_t dt = ((uint64_t)date << 18) | (uint64_t)time;
    return dt;
}

/**
 * @brief Unpack date and time from a packed 64-bit value.
 *
 * @param dt Packed date/time value.
 * @param date Output date (YYYYMMDD).
 * @param time Output time (HHMMSS).
 */
void unpackDateTime(const uint64_t* dt, uint32_t* date, uint32_t* time) {
    *time = (*dt) & ((1ULL << 18) - 1);
    *date = (*dt) >> 18;
}

/**
 * @brief Left-pad a string with zeros.
 *
 * Example:
 * "123" -> "000123"
 *
 * @param src Input string.
 * @param dst Output buffer.
 * @param dstSize Output buffer size.
 * @param width Desired total width.
 */
void leftPadZero(const char* src, char* dst, size_t dstSize, int width) {
    snprintf(dst, dstSize, "%0*s", width, src);
}

/**
 * @brief Format integer with leading zeros.
 *
 * @param number Input number.
 * @param totalWidth Desired width.
 * @param output Output buffer.
 * @param outputSize Output buffer size.
 */
void prependZerosInt(uint64_t number, int totalWidth, char* output,
                     size_t outputSize) {
    snprintf(output, outputSize, "%0*d", totalWidth, number);
}

/**
 * @brief Format uint64_t with leading zeros.
 *
 * @param number Input number.
 * @param totalWidth Desired width.
 * @param output Output buffer.
 * @param outputSize Output buffer size.
 */
void prependZerosUInt64(uint64_t number, int totalWidth, char* output,
                        size_t outputSize) {
    snprintf(output, outputSize, "%0*" PRIu64, totalWidth, number);
}

/**
 * @brief Convert digit stream into left-aligned IPv4 display format.
 *
 * Missing octets are displayed as "---".
 * Values larger than 255 are clamped to 255.
 *
 * @param in Input digit stream.
 * @param out Output formatted IP string.
 *
 * @return true if all octets were valid, false if clamping occurred.
 */
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

/**
 * @brief Validate and normalize a 12-digit IPv4 address.
 *
 * Example:
 * "192168001001" -> "192.168.1.1"
 *
 * @param input 12-digit IPv4 string.
 * @param output Output buffer.
 * @param outputSize Output buffer size.
 *
 * @return true if input is valid.
 */
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

        STRING_TO_U32(octetStr, &octet[i]);
        // Validate IPv4 range
        if (octet[i] < 0 || octet[i] > 255)
            return false;
    }
    // Build normalized IP
    snprintf(output, outputSize, "%d.%d.%d.%d", octet[0], octet[1], octet[2],
             octet[3]);

    return true;
}

/**
 * @brief Validate a date string.
 *
 * Accepts 8 digits in YYYYMMDD format.
 *
 * @param in Input date string.
 *
 * @return true if date is valid.
 */
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

/**
 * @brief Validate a time string.
 *
 * Accepts 6 digits in HHMMSS format.
 *
 * @param in Input time string.
 *
 * @return true if time is valid.
 */
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

/**
 * @brief Skip leading zeros in-place.
 *
 * Example:
 * "000123" -> points to "123"
 *
 * @param str Input string.
 *
 * @return Pointer to first non-zero character.
 */
char* skipLeadingZeros(char* str) {
    if (str == NULL)
        return NULL;

    while (*str == '0' && *(str + 1) != '\0')
        str++;

    return str;
}

/**
 * @brief Remove leading zeros and copy result.
 *
 * @param input Input string.
 * @param output Output buffer.
 * @param outputSize Output buffer size.
 *
 * @return true on success.
 */
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

/**
 * @brief Extract BIN/IIN from PAN.
 *
 * Supports 6-digit and 8-digit BIN lengths.
 *
 * @param pan PAN string.
 * @param bin Output BIN buffer.
 * @param binSize Output buffer size.
 * @param binLen BIN length (6 or 8).
 *
 * @return true on success.
 */
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

/**
 * @brief Mask PAN digits.
 *
 * Keeps first 6 and last 4 digits visible.
 *
 * Example:
 * 6037991234567890 -> 603799******7890
 *
 * @param pan Input PAN.
 * @param masked Output buffer.
 * @param masked_size Output buffer size.
 *
 * @return 0 on success, negative error code otherwise.
 */
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

/**
 * @brief Right-pad a string to a fixed length.
 *
 * @param unpadded Source string.
 * @param unpadlength Source length.
 * @param len Target length.
 * @param padded Output buffer.
 * @param p Padding character.
 */
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

/**
 * @brief Left-pad a string to a fixed length.
 *
 * @param unpadded Source string.
 * @param unpadlength Source length.
 * @param len Target length.
 * @param padded Output buffer.
 * @param p Padding character.
 */
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

/**
 * @brief Determine bill type from bill identifier.
 *
 * Bill type is encoded in the second last digit.
 *
 * @param billId Bill identifier.
 * @param len Length of bill identifier.
 *
 * @return Bill type code.
 */
int getBillType(const char* billId, size_t len) {
    char    digit = billId[len - 2];
    uint8_t type;
    STRING_TO_U8(&digit, &type);
    return type;
}

/**
 * @brief Get organization name associated with a bill type.
 *
 * @param id Bill type.
 *
 * @return Localized organization name string.
 */
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

/**
 * @brief Calculate Mod-11 check digit.
 *
 * Uses weights 2..7 repeatedly.
 *
 * @param digits Numeric string.
 *
 * @return Computed check digit.
 */
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

/**
 * @brief Extract bill amount from payment identifier.
 *
 * Amount is derived from payment ID and multiplied by 1000.
 *
 * @param paymentId Payment identifier.
 * @param amount Output amount string.
 * @param alen Output buffer size.
 *
 * @return true.
 */
bool billExtractAmount(const char* paymentId, char* amount, size_t alen) {
    size_t len = strlen(paymentId);
    // TODO: check size of amount
    memset(amount, 0, alen);
    memcpy(amount, paymentId, len - 5);
    strcat(amount, "000");
    return true;
}

/**
 * @brief Validate a bill identifier using Mod-11 checksum.
 *
 * @param billId Bill identifier.
 *
 * @return true if bill ID checksum is valid.
 */
bool isBillIdValid(const char* billId) {
    size_t inputlen = strlen(billId);
    if (inputlen < LEN_MIN_BILL_ID || inputlen > LEN_MAX_BILL_ID) {
        return false;
    }

    DEFINE_STRING(paddedBillId, LEN_MAX_BILL_ID);
    padLeft(billId, inputlen, LEN_MAX_BILL_ID, paddedBillId, '0');

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

/**
 * @brief Validate bill ID and payment ID pair.
 *
 * Verifies Mod-11 checksum of payment ID and the combined
 * bill ID + payment ID value.
 *
 * @param billId Bill identifier.
 * @param paymentId Payment identifier.
 *
 * @return true if both validations succeed.
 */
bool isBillValid(const char* billId, const char* paymentId) {
    size_t len = strlen(paymentId);

    if (len < 6 || len > 13) {
        return false;
    }
    bool res = calculateMod11(paymentId) == (paymentId[len - 1] - '0');
    RETURN_VALUE_IF_NOT(res, true, ;, false);
    DEFINE_STRING(tmp, (LEN_MAX_PAYMENT_ID * 2 + 1));
    strcpy(tmp, billId);
    strcat(tmp, paymentId);
    len = strlen(tmp);
    return calculateMod11(tmp) == (tmp[len - 1] - '0');
}

void normalizeSsid(char* in, char* out) {
    size_t size = strlen(in);
    size_t j    = 0;
    for (size_t i = 0; in[i] && j < size - 1; i++) {
        unsigned char c = (unsigned char)in[i];

        /* Allow printable ASCII only */
        if (c >= 32 && c <= 126) {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

void getSimOpName(SimCardOp_t op, char* out, size_t size) {
    Phrases_t phrase;
    switch (op) {
    case 11:
        phrase = PHRASE_SIM_OP_MCI;
        break;
    case 35:
        phrase = PHRASE_SIM_OP_MTN;
        break;
    case 20:
        phrase = PHRASE_SIM_OP_RIGHTEL;
        break;
    case 8:
        phrase = PHRASE_SIM_OP_RIGHTEL;
        // LV_SET_TEXT(operator, "STL");
        break;
    default:
        phrase = PHRASE_NKN_OPERATOR;
    }
    memset(out, 0, size);
    snprintf(out, size, "%s", phraseGetDef(phrase));
}

void getCurrentNetRouteName(char* out, size_t size) {
    Phrases_t phrase;
    switch (settings()->terminal.netRoute) {
    case NET_ROUTE_WIFI:
        phrase = PHRASE_WIFI;
        break;
    case NET_ROUTE_CELLULAR:
        phrase = PHRASE_GPRS;
        break;

    default:
        break;
    }
    memset(out, 0, size);
    snprintf(out, size, "%s", phraseGetDef(phrase));
}

bool isStringEmpty(const char* str) { return str == NULL || str[0] == '\0'; }