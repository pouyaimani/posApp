#ifndef TMS_JSON_H
#define TMS_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tmsUpgradeInfo.h"

typedef struct {
    const char* deviceSn;
    const char* brandCode;
    const char* currentVersion;
    int         packResourceType;
    const char* imei;
    const char* iccid;
    const char* lac;
    const char* cid;
    const char* apBootVersion;
    const char* apCoreVersion;
    const char* apVivaVersion;
    const char* apManageVersion;
    const char* apAppVersion;
    const char* spBootVersion;
    const char* spCoreVersion;
    const char* spAppVersion;
    const char* romType;
} TmsDeviceInfo;

typedef struct {
    const char* deviceSn;
    const char* brandCode;
    const char* originalVersion;
    const char* currentVersion;
    int         taskId;
    bool        success;
    const char* execMessage;
    const char* requestTime;
    const char* downloadFinishTime;
    const char* installFinishTime;
} TmsUpgradeReport;

bool tmsJsonBuildUpgradeCheck(char* buffer, size_t capacity,
                              const char* deviceSn, const char* brandCode,
                              const char* version, int resourceType);
bool tmsJsonBuildDeviceInfo(char* buffer, size_t capacity,
                            const TmsDeviceInfo* info);
bool tmsJsonBuildUpgradeReport(char* buffer, size_t capacity,
                               const TmsUpgradeReport* report);
bool tmsJsonParseUpgradeResponse(const uint8_t* json, size_t length,
                                 UpgradeInfo* info);
bool tmsJsonParseCodeResponse(const uint8_t* json, size_t length, int* code);
bool tmsJsonParseTimeResponse(const uint8_t* json, size_t length,
                              char* dateTime, size_t capacity);

#endif
