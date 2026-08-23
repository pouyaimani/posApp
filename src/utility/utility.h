#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "convert.h"
#include "format.h"
#include "txn.h"

typedef enum {
    BILL_PUBLIC_SERVICE = 0,
    BILL_WATER          = 1,
    BILL_ELECTRICITY    = 2,
    BILL_GAS            = 3,
    BILL_TELEPHONE      = 4,
    BILL_MOBILE         = 5,
    BILL_MUNICIPALIT    = 6,
    BILL_MUNICIPALIT_1  = 7,
    BILL_TAX            = 8,
    BILL_DRIVE_POLIC    = 9,
    BILL_INVALID,
} BillType_t;

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
void removeNonDigits(const char* src, char* dst, size_t dst_size);

/**
 * @brief Remove all '.' characters from a string.
 *
 * Copies all characters except dots into the destination buffer.
 *
 * @param src Input string.
 * @param dst Output buffer.
 * @param dst_size Size of output buffer.
 */
void removeDots(const char* src, char* dst, size_t dst_size);

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
inline void InsertDotIP(const char* ip, char* ip_add);

/**
 * @brief Check whether a character is a decimal digit.
 *
 * @param c Character to test.
 * @return true if character is '0'..'9'.
 */
bool isDigit(char c);

/**
 * @brief Check whether a character is alphabetic.
 *
 * @param c Character to test.
 * @return true if character is A-Z or a-z.
 */
bool isAlphabetic(char c);

/**
 * @brief Append a character to the end of a string.
 *
 * @param buf Null-terminated string buffer.
 * @param buf_size Total size of buffer.
 * @param c Character to append.
 *
 * @return 0 on success, -1 if buffer is full.
 */
int appendChar(char* buf, size_t buf_size, char c);

/**
 * @brief Remove the last character from a string.
 *
 * @param buf Null-terminated string.
 *
 * @return 0 on success, -1 if string is empty.
 */
int deleteChar(char* buf);

/**
 * @brief Clear a string.
 *
 * Sets the first character to '\0'.
 *
 * @param buf String buffer.
 */
void clearStr(char* buf);

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
const char* getDayName(int y, int m, int d);

/**
 * @brief Convert YYYYMMDD date into Persian day name.
 *
 * @param date Date in YYYYMMDD format.
 * @param out Output buffer.
 * @param size Output buffer size.
 *
 * @return ERR_OK on success, error code otherwise.
 */
int8_t getNameofDay(uint32_t date, char* out, size_t size);

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
uint64_t packDateTime(uint32_t date, uint32_t time);

/**
 * @brief Unpack date and time from a packed 64-bit value.
 *
 * @param dt Packed date/time value.
 * @param date Output date (YYYYMMDD).
 * @param time Output time (HHMMSS).
 */
void unpackDateTime(const uint64_t* dt, uint32_t* date, uint32_t* time);

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
void leftPadZero(const char* src, char* dst, size_t dstSize, int width);

/**
 * @brief Format integer with leading zeros.
 *
 * @param number Input number.
 * @param totalWidth Desired width.
 * @param output Output buffer.
 * @param outputSize Output buffer size.
 */
void prependZerosInt(uint64_t number, int totalWidth, char* output,
                     size_t outputSize);

/**
 * @brief Format uint64_t with leading zeros.
 *
 * @param number Input number.
 * @param totalWidth Desired width.
 * @param output Output buffer.
 * @param outputSize Output buffer size.
 */
void prependZerosUInt64(uint64_t number, int totalWidth, char* output,
                        size_t outputSize);

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
bool ipFormatLeftAligned(const char* in, char* out);

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
bool normalizeIp(const char* input, char* output, size_t outputSize);

/**
 * @brief Validate a date string.
 *
 * Accepts 8 digits in YYYYMMDD format.
 *
 * @param in Input date string.
 *
 * @return true if date is valid.
 */
bool dateValidate(const char* in);

/**
 * @brief Validate a time string.
 *
 * Accepts 6 digits in HHMMSS format.
 *
 * @param in Input time string.
 *
 * @return true if time is valid.
 */
bool timeValidate(const char* in);

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
char* skipLeadingZeros(char* str);

/**
 * @brief Remove leading zeros and copy result.
 *
 * @param input Input string.
 * @param output Output buffer.
 * @param outputSize Output buffer size.
 *
 * @return true on success.
 */
bool removeLeadingZeros(const char* input, char* output, size_t outputSize);

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
bool extractBin(const char* pan, char* bin, size_t binSize, size_t binLen);

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
int maskPan(const char* pan, char* masked, size_t masked_size);

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
              char p);

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
             char p);

/**
 * @brief Determine bill type from bill identifier.
 *
 * Bill type is encoded in the second last digit.
 *
 * @param billId Bill identifier.
 * @return Bill type code.
 */
int getBillType(const char* billId);

/**
 * @brief Get organization name associated with a bill type.
 *
 * @param id Bill type.
 *
 * @return Localized organization name string.
 */
const char* getBillOrgName(BillType_t id);

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
bool billExtractAmount(const char* paymentId, char* amount, size_t alen);

/**
 * @brief Validate a bill identifier using Mod-11 checksum.
 *
 * @param billId Bill identifier.
 *
 * @return true if bill ID checksum is valid.
 */
bool isBillIdValid(const char* billId);

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
bool isBillValid(const char* billId, const char* paymentId);

void normalizeSsid(char* in, char* out);

void getSimOpName(SimCardOp_t op, char* out, size_t size);

void getCurrentNetRouteName(char* out, size_t size);

bool isStringEmpty(const char* str);

int maskPhoneNumber(const char* phone, char* output, size_t outputSize);

#endif