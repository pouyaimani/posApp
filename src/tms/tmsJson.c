#include "tmsJson.h"

#include <stdlib.h>
#include <string.h>

#include "utility/cJSON.h"

static bool copyPrinted(cJSON* root, char* buffer, size_t capacity) {
    char*  text;
    size_t length;
    if (root == NULL || buffer == NULL || capacity == 0u)
        return false;
    text = cJSON_PrintUnformatted(root);
    if (text == NULL)
        return false;
    length = strlen(text);
    if (length >= capacity) {
        free(text);
        return false;
    }
    memcpy(buffer, text, length + 1u);
    free(text);
    return true;
}

static bool addString(cJSON* object, const char* name, const char* value,
                      bool required) {
    if (value == NULL || value[0] == '\0')
        return !required;
    return cJSON_AddStringToObject(object, name, value) != NULL;
}

static cJSON* parseBounded(const uint8_t* json, size_t length) {
    char*  copy;
    cJSON* root;
    if (json == NULL || length == 0u)
        return NULL;
    copy = (char*)malloc(length + 1u);
    if (copy == NULL)
        return NULL;
    memcpy(copy, json, length);
    copy[length] = '\0';
    root         = cJSON_Parse(copy);
    free(copy);
    return root;
}

static void copyJsonString(cJSON* object, const char* name, char* destination,
                           size_t capacity) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsString(item) && item->valuestring != NULL && capacity > 0u) {
        strncpy(destination, item->valuestring, capacity - 1u);
        destination[capacity - 1u] = '\0';
    }
}

bool tmsJsonBuildUpgradeCheck(char* buffer, size_t capacity,
                              const char* deviceSn, const char* brandCode,
                              const char* version, int resourceType) {
    bool   ok;
    cJSON* root = cJSON_CreateObject();
    if (root == NULL)
        return false;
    ok = addString(root, "deviceSn", deviceSn, true) &&
         addString(root, "brandCode", brandCode, true) &&
         addString(root, "currentVersion", version, true) &&
         cJSON_AddNumberToObject(root, "packResourceType", resourceType) !=
             NULL &&
         copyPrinted(root, buffer, capacity);
    cJSON_Delete(root);
    return ok;
}

bool tmsJsonBuildDeviceInfo(char* buffer, size_t capacity,
                            const TmsDeviceInfo* info) {
    bool   ok;
    cJSON* root;
    cJSON* ext;
    if (info == NULL)
        return false;
    root = cJSON_CreateObject();
    ext  = cJSON_CreateObject();
    if (root == NULL || ext == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(ext);
        return false;
    }
    ok = addString(root, "deviceSn", info->deviceSn, true) &&
         addString(root, "brandCode", info->brandCode, true) &&
         addString(root, "currentVersion", info->currentVersion, true) &&
         cJSON_AddNumberToObject(root, "packResourceType",
                                 info->packResourceType) != NULL &&
         addString(ext, "imei", info->imei, false) &&
         addString(ext, "iccid", info->iccid, false) &&
         addString(ext, "lac", info->lac, false) &&
         addString(ext, "cid", info->cid, false) &&
         addString(ext, "apBootVersion", info->apBootVersion, false) &&
         addString(ext, "apCoreVersion", info->apCoreVersion, false) &&
         addString(ext, "apVivaVersion", info->apVivaVersion, false) &&
         addString(ext, "apManageVersion", info->apManageVersion, false) &&
         addString(ext, "apAppVersion", info->apAppVersion, false) &&
         addString(ext, "spBootVersion", info->spBootVersion, false) &&
         addString(ext, "spCoreVersion", info->spCoreVersion, false) &&
         addString(ext, "spAppVersion", info->spAppVersion, false) &&
         addString(ext, "romType", info->romType, false);
    if (ok) {
        cJSON_AddItemToObject(root, "deviceExtInfo", ext);
        ext = NULL;
        ok  = copyPrinted(root, buffer, capacity);
    }
    cJSON_Delete(ext);
    cJSON_Delete(root);
    return ok;
}

bool tmsJsonBuildUpgradeReport(char* buffer, size_t capacity,
                               const TmsUpgradeReport* report) {
    bool   ok;
    cJSON* root;
    if (report == NULL)
        return false;
    root = cJSON_CreateObject();
    if (root == NULL)
        return false;
    ok = addString(root, "deviceSn", report->deviceSn, true) &&
         addString(root, "brandCode", report->brandCode, true) &&
         addString(root, "originalVersion", report->originalVersion, true) &&
         addString(root, "currentVersion", report->currentVersion, true) &&
         cJSON_AddBoolToObject(root, "success", report->success) != NULL &&
         addString(root, "requestTime", report->requestTime, true) &&
         addString(root, "execMessage", report->execMessage, false) &&
         addString(root, "downloadFinishTime", report->downloadFinishTime,
                   false) &&
         addString(root, "installFinishTime", report->installFinishTime, false);
    if (ok && report->taskId > 0)
        ok = cJSON_AddNumberToObject(root, "taskId", report->taskId) != NULL;
    if (ok)
        ok = copyPrinted(root, buffer, capacity);
    cJSON_Delete(root);
    return ok;
}

bool tmsJsonParseUpgradeResponse(const uint8_t* json, size_t length,
                                 UpgradeInfo* info) {
    cJSON* root;
    cJSON* code;
    cJSON* data;
    cJSON* item;
    if (info == NULL)
        return false;
    root = parseBounded(json, length);
    if (root == NULL)
        return false;
    code = cJSON_GetObjectItemCaseSensitive(root, "code");
    data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsNumber(code)) {
        cJSON_Delete(root);
        return false;
    }
    info->responseCode = code->valueint;
    if (cJSON_IsObject(data)) {
        item = cJSON_GetObjectItemCaseSensitive(data, "isHasNewVersion");
        if (cJSON_IsBool(item) || cJSON_IsNumber(item))
            info->isHasNewVersion = cJSON_IsTrue(item) || item->valueint != 0;
        copyJsonString(data, "message", info->message, sizeof(info->message));
        copyJsonString(data, "planId", info->planId, sizeof(info->planId));
        copyJsonString(data, "version", info->version, sizeof(info->version));
        copyJsonString(data, "url", info->url, sizeof(info->url));
        copyJsonString(data, "hash", info->hash, sizeof(info->hash));
        copyJsonString(data, "ts", info->ts, sizeof(info->ts));
        item = cJSON_GetObjectItemCaseSensitive(data, "fileSize");
        if (cJSON_IsNumber(item))
            info->fileSize = item->valueint;
        else if (cJSON_IsString(item) && item->valuestring != NULL)
            info->fileSize = (int)strtol(item->valuestring, NULL, 10);
    }
    cJSON_Delete(root);
    return true;
}

bool tmsJsonParseCodeResponse(const uint8_t* json, size_t length, int* code) {
    cJSON* root;
    cJSON* item;
    if (code == NULL)
        return false;
    root = parseBounded(json, length);
    if (root == NULL)
        return false;
    item = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(item)) {
        cJSON_Delete(root);
        return false;
    }
    *code = item->valueint;
    cJSON_Delete(root);
    return true;
}

bool tmsJsonParseTimeResponse(const uint8_t* json, size_t length,
                              char* dateTime, size_t capacity) {
    cJSON* root;
    cJSON* code;
    cJSON* data;
    cJSON* ts;
    if (dateTime == NULL || capacity == 0u)
        return false;
    root = parseBounded(json, length);
    if (root == NULL)
        return false;
    code = cJSON_GetObjectItemCaseSensitive(root, "code");
    data = cJSON_GetObjectItemCaseSensitive(root, "data");
    ts   = cJSON_IsObject(data) ? cJSON_GetObjectItemCaseSensitive(data, "ts")
                                : NULL;
    if (!cJSON_IsNumber(code) || code->valueint != 0 || !cJSON_IsString(ts) ||
        ts->valuestring == NULL || strlen(ts->valuestring) >= capacity) {
        cJSON_Delete(root);
        return false;
    }
    strcpy(dateTime, ts->valuestring);
    cJSON_Delete(root);
    return true;
}
