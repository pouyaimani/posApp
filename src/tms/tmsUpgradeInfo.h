#ifndef TMS_UPGRADE_INFO_H_
#define TMS_UPGRADE_INFO_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     isHasNewVersion;
    char     message[128];
    char     planId[32];
    char     version[16];
    char     url[512];
    uint32_t fileSize;
    char     hash[64];
    int      responseCode;
    char     ts[64];
} UpgradeInfo;

#endif