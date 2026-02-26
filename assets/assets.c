#include "assets.h"
#include <stdlib.h>
#include "logger.h"

//app address in ram
#define MCU_BASE_ADDR  (0x1000000)
//end

#define LVGL_START_ADDR  0x1D8000
static const char *getImgVer(void)
{
    return (const char *)(LVGL_START_ADDR + MCU_BASE_ADDR);
}

typedef int (*lvgl_func_t)(char *, void **);
void *getImgAddr(const char *name)
{
    int ret = 0;
    void *p = NULL;
    lvgl_func_t fun = *((unsigned int *)(LVGL_START_ADDR + 0x200 + MCU_BASE_ADDR));

    ret = fun(name, &p);
    if(ret == 0)
    {
        return p;
    }
    else
    {
        return "";
    }
}