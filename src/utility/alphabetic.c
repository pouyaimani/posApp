#include "alphabetic.h"

#include <string.h>
#include <ctype.h>
#include "dev/dev.h"
#include "event.h"

static uint16_t minElapsedTimeMs;
static uint16_t count;
static uint32_t prevTimeMs;
static Key_t  prevKey;
static bool upperCase;
static bool backSpace;

static Alphebatic alpheb;

/* Static key map table */
static const char *keyMap[] =
{
    [KEY_0] = ",*#0",
    [KEY_1] = "qz.1",
    [KEY_2] = "abc2",
    [KEY_3] = "def3",
    [KEY_4] = "ghi4",
    [KEY_5] = "jkl5",
    [KEY_6] = "mno6",
    [KEY_7] = "pqrs7",
    [KEY_8] = "tuv8",
    [KEY_9] = "wxyz9"
};

/* Internal helper */
static int isAlphabetic(char ch)
{
    return isalpha((unsigned char)ch);
}

static Alphebatic * reset()
{
    count = 0;
    prevTimeMs = 0;
    prevKey = KEY_NONE;
    upperCase = false;
    backSpace = false;
    return &alpheb;
}

static void setMinimumDelay(uint16_t delay)
{
    minElapsedTimeMs = delay;
}

static Alphebatic * enableUpperCase(bool en)
{
    upperCase = en;
    return &alpheb;
}

static void addKey(char *str, size_t maxLen,
                                Key_t key)
{
    if (key == KEY_NONE)
        return;

    uint32_t elapsedTimeMs = GET_TICK() - prevTimeMs;

    /* Handle backspace */
    if (key == KEY_CLEAR)
    {
        size_t len = strlen(str);
        if (len > 0)
        {
            str[len - 1] = '\0';
        }
        count = 0;
        backSpace = true;
        return;
    }

    const char *map = keyMap[key];
    if (!map)
        return;

    size_t len = strlen(str);
    size_t mapLen = strlen(map);

    if (elapsedTimeMs > minElapsedTimeMs ||
        key != prevKey ||
        backSpace)
    {
        count = 0;
        backSpace = false;

        if (len < maxLen - 1)
        {
            char ch = map[count];
            if (upperCase && isAlphabetic(ch))
                ch = toupper((unsigned char)ch);

            str[len] = ch;
            str[len + 1] = '\0';
        }
    }
    else
    {
        count++;
        if (count >= mapLen)
            count = 0;

        if (len > 0)
        {
            str[len - 1] = '\0';
            len--;

            char ch = map[count];
            if (upperCase && isAlphabetic(ch))
                ch = toupper((unsigned char)ch);

            str[len] = ch;
            str[len + 1] = '\0';
        }
    }

    prevTimeMs = GET_TICK();
    prevKey = key;
}

static void init() {
    alpheb.addKey = addKey;
    alpheb.enableUpperCase = enableUpperCase;
    alpheb.reset = reset;
}

Alphebatic *alphebatic() {
    CALL_ONCE(
        init();
        setMinimumDelay(1000);
    );
    return &alpheb;
}