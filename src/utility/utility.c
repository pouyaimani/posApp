#include "utility.h"
#include <stdbool.h>

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