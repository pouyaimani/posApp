#include "alphabetic.h"
#include <string.h>
#include "sys/sys.h"

static uint16_t minElapsedTimeMs;
static uint16_t count;
static uint32_t prevTimeMs;
static Key_t    prevKey;
static bool     backSpace;

static Alphebatic alpheb;

/* Static key map table (Uppercase included inside map) */
static const char* keyMap[] = {[KEY_0] = ",*#0",    [KEY_1] = "QZqz.1",
                               [KEY_2] = "ABCabc2", [KEY_3] = "DEFdef3",
                               [KEY_4] = "GHIghi4", [KEY_5] = "JKLjkl5",
                               [KEY_6] = "MNOmno6", [KEY_7] = "PQRSpqrs7",
                               [KEY_8] = "TUVtuv8", [KEY_9] = "WXYZwxyz9"};

static Alphebatic* reset() {
    count      = 0;
    prevTimeMs = 0;
    prevKey    = KEY_NONE;
    backSpace  = false;
    return &alpheb;
}

static void setMinimumDelay(uint16_t delay) { minElapsedTimeMs = delay; }

static void addKey(char* str, size_t maxLen, Key_t key) {
    if (key == KEY_NONE)
        return;

    uint32_t elapsedTimeMs = GET_TICK() - prevTimeMs;

    /* Handle backspace */
    if (key == KEY_CLEAR) {
        size_t len = strlen(str);
        if (len > 0) {
            str[len - 1] = '\0';
        }
        count     = 0;
        backSpace = true;
        return;
    }

    const char* map = keyMap[key];
    if (!map)
        return;

    size_t len    = strlen(str);
    size_t mapLen = strlen(map);

    if (elapsedTimeMs > minElapsedTimeMs || key != prevKey || backSpace) {
        count     = 0;
        backSpace = false;

        if (len < maxLen - 1) {
            str[len]     = map[count];
            str[len + 1] = '\0';
        }
    } else {
        count++;
        if (count >= mapLen)
            count = 0;

        if (len > 0) {
            str[len - 1] = '\0';
            len--;

            str[len]     = map[count];
            str[len + 1] = '\0';
        }
    }

    prevTimeMs = GET_TICK();
    prevKey    = key;
}

static void init() {
    alpheb.addKey = addKey;
    alpheb.reset  = reset;
}

Alphebatic* alphebatic() {
    CALL_ONCE(init(); setMinimumDelay(1000););
    return &alpheb;
}
