#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "dt.h"
#include "sys/sys.h"

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
    int y = (int)(date / 10000);
    int m = (int)((date % 10000) / 100);
    int d = (int)(date % 100);
    snprintf(out, size, "%s", getDayName(y, m, d));
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
bool getDateTimeBcd(uint8_t date_bcd[3], uint8_t time_bcd[3]) {
    LOG_TRACE("txnEmvGetDateTimeBcd: start");

    if (!date_bcd || !time_bcd) {
        LOG_TRACE("txnEmvGetDateTimeBcd: ERROR - NULL output buffer");
        return false;
    }

    DateTime* dt = OOP_CALL(sys(), getDateTime);

    if (!dt) {
        LOG_TRACE("txnEmvGetDateTimeBcd: ERROR - RTC returned NULL");
        return false;
    }

    /* Validate string termination: YYMMDD / HHMMSS. */
    if (dt->date[6] != '\0' || dt->time[6] != '\0') {
        LOG_TRACE("txnEmvGetDateTimeBcd: ERROR - invalid RTC string length");
        return false;
    }

    LOG_TRACE("txnEmvGetDateTimeBcd: RTC date=%s time=%s", dt->date, dt->time);

    /* Validate all characters are decimal digits. */
    for (size_t i = 0u; i < 6u; ++i) {
        if (dt->date[i] < '0' || dt->date[i] > '9') {
            LOG_TRACE("txnEmvGetDateTimeBcd: ERROR - "
                      "invalid date digit index=%u value=0x%02X",
                      (unsigned int)i, (unsigned int)(uint8_t)dt->date[i]);
            return false;
        }

        if (dt->time[i] < '0' || dt->time[i] > '9') {
            LOG_TRACE("txnEmvGetDateTimeBcd: ERROR - "
                      "invalid time digit index=%u value=0x%02X",
                      (unsigned int)i, (unsigned int)(uint8_t)dt->time[i]);
            return false;
        }
    }

    /* Parse date: YYMMDD. */
    unsigned int month = (unsigned int)(dt->date[2] - '0') * 10u +
                         (unsigned int)(dt->date[3] - '0');

    unsigned int day = (unsigned int)(dt->date[4] - '0') * 10u +
                       (unsigned int)(dt->date[5] - '0');

    /* Parse time: HHMMSS. */
    unsigned int hour = (unsigned int)(dt->time[0] - '0') * 10u +
                        (unsigned int)(dt->time[1] - '0');

    unsigned int minute = (unsigned int)(dt->time[2] - '0') * 10u +
                          (unsigned int)(dt->time[3] - '0');

    unsigned int second = (unsigned int)(dt->time[4] - '0') * 10u +
                          (unsigned int)(dt->time[5] - '0');

    /* Validate basic calendar and clock ranges. */
    if (month < 1u || month > 12u || day < 1u || day > 31u || hour > 23u ||
        minute > 59u || second > 59u) {

        LOG_TRACE("txnEmvGetDateTimeBcd: ERROR - "
                  "invalid date/time range "
                  "month=%u day=%u hour=%u minute=%u second=%u",
                  month, day, hour, minute, second);
        return false;
    }

    /* Convert ASCII digits to packed BCD. */
    uint8_t date[3];
    uint8_t time[3];

    for (size_t i = 0u; i < 3u; ++i) {
        size_t index = i * 2u;

        date[i] = (uint8_t)(((dt->date[index] - '0') << 4) |
                            (dt->date[index + 1u] - '0'));

        time[i] = (uint8_t)(((dt->time[index] - '0') << 4) |
                            (dt->time[index + 1u] - '0'));
    }

    memcpy(date_bcd, date, sizeof(date));
    memcpy(time_bcd, time, sizeof(time));

    LOG_TRACE("txnEmvGetDateTimeBcd: BCD date=%02X%02X%02X "
              "time=%02X%02X%02X",
              (unsigned int)date_bcd[0], (unsigned int)date_bcd[1],
              (unsigned int)date_bcd[2], (unsigned int)time_bcd[0],
              (unsigned int)time_bcd[1], (unsigned int)time_bcd[2]);

    LOG_TRACE("txnEmvGetDateTimeBcd: success");
    return true;
}
