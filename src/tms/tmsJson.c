#include "tmsJson.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "sys/sys.h"
#include "utility/cJSON.h"

/*
 * TMS JSON diagnostics:
 * - Log field names, lengths, response codes and allocation failures.
 * - Never log entire JSON payloads or device identifiers (SN/IMEI/ICCID).
 * - Keep logging outside high-frequency loops.
 */

/* -----------------------------------------------------------
 * JSON builder helpers
 * ----------------------------------------------------------- */

static bool validateOutput(char* buffer, size_t capacity,
                           const char* operation) {
    if (buffer == NULL || capacity == 0u) {
        LOG_ERROR("TMS JSON %s: invalid output buffer present=%d capacity=%lu",
                  operation, (int)(buffer != NULL), (unsigned long)capacity);
        return false;
    }

    /* A failed build must not leave the previous request in the buffer. */
    buffer[0] = '\0';
    LOG_TRACE("TMS JSON %s: output capacity=%lu", operation,
              (unsigned long)capacity);
    return true;
}

/**
 * @brief Serialize a cJSON object into a caller-owned buffer.
 *
 * cJSON_PrintUnformatted() allocates memory; this project must configure
 * cJSON's allocation hooks consistently with MEM_FREE, as in the original.
 */
static bool copyPrinted(cJSON* root, char* buffer, size_t capacity) {
    char*  text;
    size_t length;

    LOG_TRACE("TMS JSON copyPrinted(): begin capacity=%lu",
              (unsigned long)capacity);

    if (root == NULL || buffer == NULL || capacity == 0u) {
        LOG_ERROR("TMS JSON copyPrinted(): invalid args root=%d buffer=%d "
                  "capacity=%lu",
                  (int)(root != NULL), (int)(buffer != NULL),
                  (unsigned long)capacity);
        return false;
    }

    text = cJSON_PrintUnformatted(root);
    if (text == NULL) {
        LOG_ERROR("TMS JSON copyPrinted(): serialization/allocation failed");
        return false;
    }

    length = strlen(text);
    LOG_TRACE("TMS JSON copyPrinted(): serialized length=%lu capacity=%lu",
              (unsigned long)length, (unsigned long)capacity);

    if (length >= capacity) {
        LOG_ERROR(
            "TMS JSON copyPrinted(): buffer too small length=%lu capacity=%lu",
            (unsigned long)length, (unsigned long)capacity);
        MEM_FREE(text);
        return false;
    }

    memcpy(buffer, text, length + 1u);
    MEM_FREE(text);

    LOG_TRACE("TMS JSON copyPrinted(): success bytes=%lu",
              (unsigned long)length);
    return true;
}

/**
 * @brief Add a string field. Optional missing/empty values are omitted.
 *
 * Log only the field name and length, not the value: some fields contain
 * persistent device identifiers or other potentially sensitive information.
 */
static bool addString(cJSON* object, const char* name, const char* value,
                      bool required) {
    if (object == NULL || name == NULL) {
        LOG_ERROR("TMS JSON addString(): invalid target/name target=%d name=%d",
                  (int)(object != NULL), (int)(name != NULL));
        return false;
    }

    if (value == NULL || value[0] == '\0') {
        if (required) {
            LOG_ERROR("TMS JSON addString(): required field '%s' missing/empty",
                      name);
            return false;
        }
        LOG_TRACE("TMS JSON addString(): skipping optional field '%s'", name);
        return true;
    }

    if (cJSON_AddStringToObject(object, name, value) == NULL) {
        LOG_ERROR("TMS JSON addString(): failed to add field '%s'", name);
        return false;
    }

    LOG_TRACE("TMS JSON addString(): added '%s' length=%lu", name,
              (unsigned long)strlen(value));
    return true;
}

static bool addNumber(cJSON* object, const char* name, int value) {
    if (object == NULL || name == NULL) {
        LOG_ERROR("TMS JSON addNumber(): invalid target/name");
        return false;
    }
    if (cJSON_AddNumberToObject(object, name, value) == NULL) {
        LOG_ERROR("TMS JSON addNumber(): failed to add '%s' value=%d", name,
                  value);
        return false;
    }
    LOG_TRACE("TMS JSON addNumber(): added '%s' value=%d", name, value);
    return true;
}

static bool addBool(cJSON* object, const char* name, bool value) {
    if (object == NULL || name == NULL) {
        LOG_ERROR("TMS JSON addBool(): invalid target/name");
        return false;
    }
    if (cJSON_AddBoolToObject(object, name, value) == NULL) {
        LOG_ERROR("TMS JSON addBool(): failed to add '%s'", name);
        return false;
    }
    LOG_TRACE("TMS JSON addBool(): added '%s' value=%d", name, (int)value);
    return true;
}

/* -----------------------------------------------------------
 * JSON parser helpers
 * ----------------------------------------------------------- */

/**
 * @brief Parse an HTTP body using an allocated, null-terminated copy.
 *
 * HTTP bodies are not guaranteed to be null-terminated. Reject embedded
 * NULs to avoid accepting a valid JSON prefix with ignored trailing bytes.
 */
static cJSON* parseBounded(const uint8_t* json, size_t length) {
    char*  copy;
    cJSON* root;

    LOG_TRACE("TMS JSON parseBounded(): begin length=%lu",
              (unsigned long)length);

    if (json == NULL || length == 0u) {
        LOG_ERROR(
            "TMS JSON parseBounded(): empty/NULL input ptrSet=%d length=%lu",
            (int)(json != NULL), (unsigned long)length);
        return NULL;
    }

    if (length == SIZE_MAX) {
        LOG_ERROR("TMS JSON parseBounded(): length overflow");
        return NULL;
    }

    if (memchr(json, '\0', length) != NULL) {
        LOG_ERROR("TMS JSON parseBounded(): embedded NUL in HTTP body");
        return NULL;
    }

    copy = (char*)MEM_ALLOC(length + 1u);
    if (copy == NULL) {
        LOG_ERROR("TMS JSON parseBounded(): allocation failed bytes=%lu",
                  (unsigned long)(length + 1u));
        return NULL;
    }

    memcpy(copy, json, length);
    copy[length] = '\0';
    root         = cJSON_Parse(copy);
    MEM_FREE(copy);

    if (root == NULL) {
        LOG_ERROR(
            "TMS JSON parseBounded(): JSON syntax parse failed length=%lu",
            (unsigned long)length);
        return NULL;
    }

    if (!cJSON_IsObject(root)) {
        LOG_ERROR("TMS JSON parseBounded(): top-level JSON is not an object");
        cJSON_Delete(root);
        return NULL;
    }

    LOG_TRACE("TMS JSON parseBounded(): parsed root object successfully");
    return root;
}

/**
 * @brief Copy a JSON string into a fixed-size destination.
 *
 * Missing fields produce empty strings; truncations are explicitly logged.
 * Truncation continues to be non-fatal to preserve previous parser behavior.
 */
static void copyJsonString(cJSON* object, const char* name, char* destination,
                           size_t capacity) {
    cJSON* item;
    size_t sourceLength;

    if (object == NULL || name == NULL || destination == NULL ||
        capacity == 0u) {
        LOG_ERROR("TMS JSON copyJsonString(): invalid args object=%d name=%d "
                  "dest=%d capacity=%lu",
                  (int)(object != NULL), (int)(name != NULL),
                  (int)(destination != NULL), (unsigned long)capacity);
        return;
    }

    destination[0] = '\0';
    item           = cJSON_GetObjectItemCaseSensitive(object, name);

    if (item == NULL) {
        LOG_TRACE("TMS JSON copyJsonString(): field '%s' absent", name);
        return;
    }

    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        LOG_ERROR("TMS JSON copyJsonString(): field '%s' is not a string",
                  name);
        return;
    }

    sourceLength = strlen(item->valuestring);
    if (sourceLength >= capacity) {
        LOG_ERROR("TMS JSON copyJsonString(): field '%s' truncated "
                  "sourceLen=%lu capacity=%lu",
                  name, (unsigned long)sourceLength, (unsigned long)capacity);
        memcpy(destination, item->valuestring, capacity - 1u);
        destination[capacity - 1u] = '\0';
        return;
    }

    memcpy(destination, item->valuestring, sourceLength + 1u);
    LOG_TRACE("TMS JSON copyJsonString(): copied '%s' length=%lu", name,
              (unsigned long)sourceLength);
}

/* -----------------------------------------------------------
 * Request builders
 * ----------------------------------------------------------- */

bool tmsJsonBuildUpgradeCheck(char* buffer, size_t capacity,
                              const char* deviceSn, const char* brandCode,
                              const char* version, int resourceType) {
    bool   ok;
    cJSON* root;

    LOG_TRACE("TMS JSON tmsJsonBuildUpgradeCheck(): begin resourceType=%d",
              resourceType);

    if (!validateOutput(buffer, capacity, "UpgradeCheck"))
        return false;

    root = cJSON_CreateObject();
    if (root == NULL) {
        LOG_ERROR(
            "TMS JSON tmsJsonBuildUpgradeCheck(): root allocation failed");
        return false;
    }

    ok = addString(root, "deviceSn", deviceSn, true) &&
         addString(root, "brandCode", brandCode, true) &&
         addString(root, "currentVersion", version, true) &&
         addNumber(root, "packResourceType", resourceType) &&
         copyPrinted(root, buffer, capacity);

    cJSON_Delete(root);

    if (!ok)
        LOG_ERROR("TMS JSON tmsJsonBuildUpgradeCheck(): build failed");
    else
        LOG_TRACE("TMS JSON tmsJsonBuildUpgradeCheck(): success jsonLen=%lu",
                  (unsigned long)strlen(buffer));
    return ok;
}

bool tmsJsonBuildDeviceInfo(char* buffer, size_t capacity,
                            const TmsDeviceInfo* info) {
    bool   ok;
    cJSON* root;
    cJSON* ext;

    LOG_TRACE("TMS JSON tmsJsonBuildDeviceInfo(): begin");

    if (!validateOutput(buffer, capacity, "DeviceInfo"))
        return false;
    if (info == NULL) {
        LOG_ERROR("TMS JSON tmsJsonBuildDeviceInfo(): info is NULL");
        return false;
    }

    root = cJSON_CreateObject();
    ext  = cJSON_CreateObject();
    if (root == NULL || ext == NULL) {
        LOG_ERROR("TMS JSON tmsJsonBuildDeviceInfo(): JSON allocation failed "
                  "root=%d ext=%d",
                  (int)(root != NULL), (int)(ext != NULL));
        cJSON_Delete(root);
        cJSON_Delete(ext);
        return false;
    }

    LOG_TRACE("TMS JSON tmsJsonBuildDeviceInfo(): adding device fields");
    ok = addString(root, "deviceSn", info->deviceSn, true) &&
         addString(root, "brandCode", info->brandCode, true) &&
         addString(root, "currentVersion", info->currentVersion, true) &&
         addNumber(root, "packResourceType", info->packResourceType) &&
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
        LOG_TRACE("TMS JSON tmsJsonBuildDeviceInfo(): attaching deviceExtInfo");
        cJSON_AddItemToObject(root, "deviceExtInfo", ext);
        ext = NULL; /* root now owns ext */
        ok  = copyPrinted(root, buffer, capacity);
    }

    cJSON_Delete(ext);
    cJSON_Delete(root);

    if (!ok)
        LOG_ERROR("TMS JSON tmsJsonBuildDeviceInfo(): build failed");
    else
        LOG_TRACE("TMS JSON tmsJsonBuildDeviceInfo(): success jsonLen=%lu",
                  (unsigned long)strlen(buffer));
    return ok;
}

bool tmsJsonBuildUpgradeReport(char* buffer, size_t capacity,
                               const TmsUpgradeReport* report) {
    bool   ok;
    cJSON* root;

    LOG_TRACE("TMS JSON tmsJsonBuildUpgradeReport(): begin");

    if (!validateOutput(buffer, capacity, "UpgradeReport"))
        return false;
    if (report == NULL) {
        LOG_ERROR("TMS JSON tmsJsonBuildUpgradeReport(): report is NULL");
        return false;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        LOG_ERROR(
            "TMS JSON tmsJsonBuildUpgradeReport(): root allocation failed");
        return false;
    }

    LOG_TRACE("TMS JSON tmsJsonBuildUpgradeReport(): success=%d taskId=%d",
              (int)report->success, (int)report->taskId);

    ok = addString(root, "deviceSn", report->deviceSn, true) &&
         addString(root, "brandCode", report->brandCode, true) &&
         addString(root, "originalVersion", report->originalVersion, true) &&
         addString(root, "currentVersion", report->currentVersion, true) &&
         addBool(root, "success", report->success) &&
         addString(root, "requestTime", report->requestTime, true) &&
         addString(root, "execMessage", report->execMessage, false) &&
         addString(root, "downloadFinishTime", report->downloadFinishTime,
                   false) &&
         addString(root, "installFinishTime", report->installFinishTime, false);

    if (ok && report->taskId > 0) {
        ok = addNumber(root, "taskId", report->taskId);
    } else if (ok) {
        LOG_TRACE("TMS JSON tmsJsonBuildUpgradeReport(): taskId omitted (<=0)");
    }

    if (ok)
        ok = copyPrinted(root, buffer, capacity);

    cJSON_Delete(root);

    if (!ok)
        LOG_ERROR("TMS JSON tmsJsonBuildUpgradeReport(): build failed");
    else
        LOG_TRACE("TMS JSON tmsJsonBuildUpgradeReport(): success jsonLen=%lu",
                  (unsigned long)strlen(buffer));
    return ok;
}

/* -----------------------------------------------------------
 * Parsed upgrade information diagnostics
 * ----------------------------------------------------------- */

static void printUpdateInfo(const UpgradeInfo* info) {
    if (info == NULL) {
        LOG_ERROR("TMS JSON printUpdateInfo(): info is NULL");
        return;
    }

    LOG_TRACE("TMS JSON ===== Upgrade response summary =====");
    LOG_TRACE("TMS JSON responseCode=%d hasNewVersion=%d", info->responseCode,
              (int)info->isHasNewVersion);
    LOG_TRACE("TMS JSON planId=%s version=%s", info->planId, info->version);
    LOG_TRACE("TMS JSON fileSize=%d timestamp=%s", info->fileSize, info->ts);
    LOG_TRACE("TMS JSON messageLen=%lu urlLen=%lu hashLen=%lu",
              (unsigned long)strlen(info->message),
              (unsigned long)strlen(info->url),
              (unsigned long)strlen(info->hash));
    LOG_TRACE("TMS JSON ====================================");
}

/* -----------------------------------------------------------
 * Response parsers
 * ----------------------------------------------------------- */

bool tmsJsonParseUpgradeResponse(const uint8_t* json, size_t length,
                                 UpgradeInfo* info) {
    cJSON* root;
    cJSON* code;
    cJSON* data;
    cJSON* item;

    LOG_TRACE("TMS JSON tmsJsonParseUpgradeResponse(): begin length=%lu",
              (unsigned long)length);

    if (info == NULL) {
        LOG_ERROR("TMS JSON tmsJsonParseUpgradeResponse(): info is NULL");
        return false;
    }

    root = parseBounded(json, length);
    if (root == NULL) {
        LOG_ERROR("TMS JSON tmsJsonParseUpgradeResponse(): invalid JSON");
        return false;
    }

    code = cJSON_GetObjectItemCaseSensitive(root, "code");
    data = cJSON_GetObjectItemCaseSensitive(root, "data");

    if (!cJSON_IsNumber(code)) {
        LOG_ERROR("TMS JSON tmsJsonParseUpgradeResponse(): missing/non-numeric "
                  "'code'");
        cJSON_Delete(root);
        return false;
    }

    /* Always initialize fields before optional data is copied/logged. */
    memset(info, 0, sizeof(*info));
    info->responseCode = code->valueint;
    LOG_TRACE("TMS JSON tmsJsonParseUpgradeResponse(): responseCode=%d",
              info->responseCode);

    if (cJSON_IsObject(data)) {
        LOG_TRACE(
            "TMS JSON tmsJsonParseUpgradeResponse(): parsing data object");

        item = cJSON_GetObjectItemCaseSensitive(data, "isHasNewVersion");
        if (cJSON_IsBool(item) || cJSON_IsNumber(item)) {
            info->isHasNewVersion = cJSON_IsTrue(item) || item->valueint != 0;
            LOG_TRACE(
                "TMS JSON tmsJsonParseUpgradeResponse(): isHasNewVersion=%d",
                (int)info->isHasNewVersion);
        } else if (item != NULL) {
            LOG_ERROR("TMS JSON tmsJsonParseUpgradeResponse(): invalid "
                      "'isHasNewVersion' type");
        } else {
            LOG_TRACE("TMS JSON tmsJsonParseUpgradeResponse(): "
                      "'isHasNewVersion' absent");
        }

        copyJsonString(data, "message", info->message, sizeof(info->message));
        copyJsonString(data, "planId", info->planId, sizeof(info->planId));
        copyJsonString(data, "version", info->version, sizeof(info->version));
        copyJsonString(data, "url", info->url, sizeof(info->url));
        copyJsonString(data, "hash", info->hash, sizeof(info->hash));
        copyJsonString(data, "ts", info->ts, sizeof(info->ts));

        item = cJSON_GetObjectItemCaseSensitive(data, "fileSize");
        if (cJSON_IsNumber(item)) {
            info->fileSize = item->valueint;
            LOG_TRACE(
                "TMS JSON tmsJsonParseUpgradeResponse(): fileSize(number)=%d",
                info->fileSize);
        } else if (cJSON_IsString(item) && item->valuestring != NULL) {
            char* end = NULL;
            long  parsedSize;
            errno      = 0;
            parsedSize = strtol(item->valuestring, &end, 10);
            if (errno != 0 || end == item->valuestring || *end != '\0' ||
                parsedSize < 0 || parsedSize > INT_MAX) {
                LOG_ERROR("TMS JSON tmsJsonParseUpgradeResponse(): invalid "
                          "fileSize string");
            } else {
                info->fileSize = (int)parsedSize;
                LOG_TRACE("TMS JSON tmsJsonParseUpgradeResponse(): "
                          "fileSize(string)=%d",
                          info->fileSize);
            }
        } else if (item != NULL) {
            LOG_ERROR("TMS JSON tmsJsonParseUpgradeResponse(): unsupported "
                      "fileSize type");
        } else {
            LOG_TRACE(
                "TMS JSON tmsJsonParseUpgradeResponse(): fileSize absent");
        }

        printUpdateInfo(info);
    } else if (data == NULL) {
        LOG_TRACE(
            "TMS JSON tmsJsonParseUpgradeResponse(): data absent (code=%d)",
            info->responseCode);
    } else {
        LOG_ERROR(
            "TMS JSON tmsJsonParseUpgradeResponse(): 'data' is not an object");
        /* Preserve original behavior: a numeric code is sufficient. */
    }

    cJSON_Delete(root);
    LOG_TRACE(
        "TMS JSON tmsJsonParseUpgradeResponse(): parsed successfully code=%d",
        info->responseCode);
    return true;
}

bool tmsJsonParseCodeResponse(const uint8_t* json, size_t length, int* code) {
    cJSON* root;
    cJSON* item;

    LOG_TRACE("TMS JSON tmsJsonParseCodeResponse(): begin length=%lu",
              (unsigned long)length);

    if (code == NULL) {
        LOG_ERROR("TMS JSON tmsJsonParseCodeResponse(): output code is NULL");
        return false;
    }

    root = parseBounded(json, length);
    if (root == NULL) {
        LOG_ERROR("TMS JSON tmsJsonParseCodeResponse(): invalid JSON");
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(item)) {
        LOG_ERROR(
            "TMS JSON tmsJsonParseCodeResponse(): missing/non-numeric 'code'");
        cJSON_Delete(root);
        return false;
    }

    *code = item->valueint;
    LOG_TRACE("TMS JSON tmsJsonParseCodeResponse(): parsed code=%d", *code);
    cJSON_Delete(root);
    return true;
}

bool tmsJsonParseTimeResponse(const uint8_t* json, size_t length,
                              char* dateTime, size_t capacity) {
    cJSON* root;
    cJSON* code;
    cJSON* data;
    cJSON* ts;
    size_t tsLength;

    LOG_TRACE(
        "TMS JSON tmsJsonParseTimeResponse(): begin length=%lu capacity=%lu",
        (unsigned long)length, (unsigned long)capacity);

    if (dateTime == NULL || capacity == 0u) {
        LOG_ERROR("TMS JSON tmsJsonParseTimeResponse(): invalid output buffer "
                  "present=%d capacity=%lu",
                  (int)(dateTime != NULL), (unsigned long)capacity);
        return false;
    }

    root = parseBounded(json, length);
    if (root == NULL) {
        LOG_ERROR("TMS JSON tmsJsonParseTimeResponse(): invalid JSON");
        return false;
    }

    code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(code)) {
        LOG_ERROR(
            "TMS JSON tmsJsonParseTimeResponse(): missing/non-numeric 'code'");
        cJSON_Delete(root);
        return false;
    }

    LOG_TRACE("TMS JSON tmsJsonParseTimeResponse(): responseCode=%d",
              code->valueint);
    if (code->valueint != 0) {
        LOG_ERROR("TMS JSON tmsJsonParseTimeResponse(): server rejected time "
                  "sync code=%d",
                  code->valueint);
        cJSON_Delete(root);
        return false;
    }

    data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        LOG_ERROR("TMS JSON tmsJsonParseTimeResponse(): missing/invalid 'data' "
                  "object");
        cJSON_Delete(root);
        return false;
    }

    ts = cJSON_GetObjectItemCaseSensitive(data, "ts");
    if (!cJSON_IsString(ts) || ts->valuestring == NULL) {
        LOG_ERROR(
            "TMS JSON tmsJsonParseTimeResponse(): missing/invalid 'ts' string");
        cJSON_Delete(root);
        return false;
    }

    tsLength = strlen(ts->valuestring);
    if (tsLength >= capacity) {
        LOG_ERROR("TMS JSON tmsJsonParseTimeResponse(): timestamp exceeds "
                  "buffer tsLen=%lu capacity=%lu",
                  (unsigned long)tsLength, (unsigned long)capacity);
        cJSON_Delete(root);
        return false;
    }

    memcpy(dateTime, ts->valuestring, tsLength + 1u);
    LOG_TRACE("TMS JSON tmsJsonParseTimeResponse(): success timestamp=%s",
              dateTime);
    cJSON_Delete(root);
    return true;
}
