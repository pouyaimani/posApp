#ifndef DT_H_
#define DT_H_

/**
 * Get current RTC date and time in packed BCD format.
 *
 * Input:
 *   date = "YYMMDD"
 *   time = "HHMMSS"
 *
 * Output:
 *   date_bcd[3] = {YY, MM, DD}
 *   time_bcd[3] = {HH, MM, SS}
 *
 * Returns true on success, false on invalid input.
 */
bool getDateTimeBcd(uint8_t date_bcd[3], uint8_t time_bcd[3]);

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

#endif