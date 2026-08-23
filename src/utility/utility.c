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
int getBillType(const char* billId) {
    size_t len = strlen(billId);
    LOG_TRACE("getBillType: billId = %s", billId);
    DEFINE_STRING(digit, 4);
    digit[0] = billId[len - 2];
    LOG_TRACE("getBillType: bill type digit = %s", digit);
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
    LOG_TRACE("getBillOrgName: bill id = %d", id);
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
 * @brief Check whether a string contains only decimal digits.
 *
 * @param digits Numeric string.
 * @param len    String length.
 *
 * @return true if all characters are decimal digits.
 */
static bool isNumeric(const char* digits, size_t len) {
    for (size_t i = 0U; i < len; ++i) {
        if (digits[i] < '0' || digits[i] > '9') {
            return false;
        }
    }

    return true;
}

/**
 * @brief Calculate Mod-11 check digit for a numeric string.
 *
 * The supplied string must NOT contain the check digit.
 *
 * Weights are applied from right to left:
 *
 *     2, 3, 4, 5, 6, 7, 2, 3, ...
 *
 * The resulting remainder is converted according to the
 * Iranian Mod-11 check-digit rule:
 *
 *     remainder 0 or 1 -> check digit 0
 *     otherwise        -> 11 - remainder
 *
 * @param digits Numeric string without check digit.
 * @param len    Number of digits to process.
 *
 * @return Calculated check digit.
 */
static uint8_t calculateMod11(const char* digits, size_t len) {
    uint32_t sum    = 0U;
    uint32_t weight = 2U;

    while (len > 0U) {
        --len;

        sum += (uint32_t)(digits[len] - '0') * weight;

        ++weight;

        if (weight > 7U) {
            weight = 2U;
        }
    }

    const uint32_t remainder = sum % 11U;

    return (remainder == 0U || remainder == 1U) ? 0U
                                                : (uint8_t)(11U - remainder);
}

/**
 * @brief Validate the check digit of a numeric string.
 *
 * The last digit is treated as the check digit and is not included
 * in the Mod-11 calculation.
 *
 * @param digits Numeric string containing check digit.
 *
 * @return true if the check digit is valid.
 */
static bool isCheckDigitValid(const char* digits) {
    if (digits == NULL) {
        return false;
    }

    const size_t len = strlen(digits);

    if (len < 2U) {
        return false;
    }

    if (!isNumeric(digits, len)) {
        return false;
    }

    const uint8_t expected = (uint8_t)(digits[len - 1U] - '0');

    const uint8_t calculated = calculateMod11(digits, len - 1U);

    return expected == calculated;
}

/**
 * @brief Validate a bill identifier.
 *
 * Validates the format and Mod-11 check digit of the bill ID.
 *
 * @param billId Bill identifier.
 *
 * @return true if the bill ID is valid.
 */
bool isBillIdValid(const char* billId) {
    if (billId == NULL) {
        return false;
    }

    const size_t len = strlen(billId);

    if (len < LEN_MIN_BILL_ID || len > LEN_MAX_BILL_ID) {
        return false;
    }

    return isCheckDigitValid(billId);
}

/**
 * @brief Validate payment ID control digit #1.
 *
 * Control digit #1 is the second-last digit of the payment ID.
 * It is calculated using all payment ID digits preceding it.
 *
 * @param paymentId Payment identifier.
 *
 * @return true if control digit #1 is valid.
 */
static bool isPaymentControlDigit1Valid(const char* paymentId) {
    const size_t len = strlen(paymentId);

    if (len < 3U) {
        return false;
    }

    const uint8_t expected = (uint8_t)(paymentId[len - 2U] - '0');

    const uint8_t calculated = calculateMod11(paymentId, len - 2U);

    return expected == calculated;
}

/**
 * @brief Remove leading zeroes from a numeric string.
 *
 * The input string is not modified.
 *
 * At least one digit is always retained.
 *
 * Example:
 *
 *     "0001234" -> "1234"
 *     "0000"    -> "0"
 *
 * @param value  Input string.
 * @param length Input length.
 */
static void trimLeadingZeros(const char** value, size_t* length) {
    while (*length > 1U && **value == '0') {
        ++(*value);
        --(*length);
    }
}

/**
 * @brief Calculate payment ID control digit #2.
 *
 * The calculation is performed over:
 *
 *     trim(billId) +
 *     trim(paymentId without control digit #2)
 *
 * The two strings are processed from right to left without
 * constructing a temporary concatenated buffer.
 *
 * @param billId     Bill identifier.
 * @param paymentId  Payment identifier.
 *
 * @return Calculated control digit #2.
 */
static uint8_t calculatePaymentControlDigit2(const char* billId,
                                             const char* paymentId) {
    const char* bill    = billId;
    const char* payment = paymentId;

    size_t billLen    = strlen(billId);
    size_t paymentLen = strlen(paymentId);

    /*
     * Leading zeroes must be removed independently.
     */
    trimLeadingZeros(&bill, &billLen);
    trimLeadingZeros(&payment, &paymentLen);

    /*
     * Remove payment control digit #2.
     */
    --paymentLen;

    uint32_t sum    = 0U;
    uint32_t weight = 2U;

    /*
     * Payment ID is on the right side of:
     *
     *     bill + payment
     *
     * Therefore process it first.
     */
    while (paymentLen > 0U) {
        --paymentLen;

        sum += (uint32_t)(payment[paymentLen] - '0') * weight;

        ++weight;

        if (weight > 7U) {
            weight = 2U;
        }
    }

    /*
     * Continue into the bill ID.
     */
    while (billLen > 0U) {
        --billLen;

        sum += (uint32_t)(bill[billLen] - '0') * weight;

        ++weight;

        if (weight > 7U) {
            weight = 2U;
        }
    }

    const uint32_t remainder = sum % 11U;

    return (remainder == 0U || remainder == 1U) ? 0U
                                                : (uint8_t)(11U - remainder);
}

/**
 * @brief Validate payment ID control digit #2.
 *
 * @param billId     Bill identifier.
 * @param paymentId  Payment identifier.
 *
 * @return true if control digit #2 is valid.
 */
static bool isPaymentControlDigit2Valid(const char* billId,
                                        const char* paymentId) {
    const size_t paymentLen = strlen(paymentId);

    if (paymentLen < 2U) {
        return false;
    }

    const uint8_t expected = (uint8_t)(paymentId[paymentLen - 1U] - '0');

    const uint8_t calculated = calculatePaymentControlDigit2(billId, paymentId);

    return expected == calculated;
}

/**
 * @brief Validate a bill ID and its corresponding payment ID.
 *
 * Validation consists of:
 *
 * 1. Bill ID format and check digit.
 * 2. Payment ID format.
 * 3. Payment ID control digit #1.
 * 4. Payment ID control digit #2 using bill ID + payment ID.
 *
 * @param billId     Bill identifier.
 * @param paymentId  Payment identifier.
 *
 * @return true if the complete bill/payment pair is valid.
 */
bool isBillValid(const char* billId, const char* paymentId) {
    if (billId == NULL || paymentId == NULL) {
        return false;
    }

    const size_t paymentLen = strlen(paymentId);

    if (paymentLen < LEN_MIN_PAYMENT_ID || paymentLen > LEN_MAX_PAYMENT_ID) {
        return false;
    }

    if (!isNumeric(paymentId, paymentLen)) {
        return false;
    }

    if (!isBillIdValid(billId)) {
        return false;
    }

    if (!isPaymentControlDigit1Valid(paymentId)) {
        return false;
    }

    if (!isPaymentControlDigit2Valid(billId, paymentId)) {
        return false;
    }

    return true;
}

/**
 * @brief Extract bill amount from payment identifier.
 *
 * The amount is represented by the payment ID payload before
 * its final five digits and is expressed in the smallest monetary
 * unit by appending three zeroes.
 *
 * @param paymentId Payment identifier.
 * @param amount    Output amount string.
 * @param alen      Size of amount buffer in bytes.
 *
 * @return true on success, false on invalid input or insufficient
 *         output buffer capacity.
 */
bool billExtractAmount(const char* paymentId, char* amount, size_t alen) {
    if (paymentId == NULL || amount == NULL || alen == 0U) {
        return false;
    }

    const size_t len = strlen(paymentId);

    if (len < LEN_MIN_PAYMENT_ID || len > LEN_MAX_PAYMENT_ID) {
        return false;
    }

    if (!isNumeric(paymentId, len)) {
        return false;
    }

    /*
     * Number of digits representing the amount.
     */
    const size_t amountDigits = len - 5U;

    /*
     * +3 for the appended "000"
     * +1 for '\0'
     */
    const size_t requiredSize = amountDigits + 3U + 1U;

    if (alen < requiredSize) {
        return false;
    }

    memcpy(amount, paymentId, amountDigits);

    memcpy(amount + amountDigits, "000", 3U);

    amount[amountDigits + 3U] = '\0';

    return true;
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
    case OPERATOR_MCI:
        phrase = PHRASE_SIM_OP_MCI;
        break;
    case OPERATOR_MTN:
        phrase = PHRASE_SIM_OP_MTN;
        break;
    case OPERATOR_RIGHTEL:
        phrase = PHRASE_SIM_OP_RIGHTEL;
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

int maskPhoneNumber(const char* phone, char* output, size_t outputSize) {
    if (!phone || !output || outputSize < 13)
        return -1;

    if (strlen(phone) != 11)
        return -1;

    snprintf(output, outputSize, "%.4s-***-%.4s", phone, phone + 7);

    return 0;
}