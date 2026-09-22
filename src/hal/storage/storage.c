#include "storage.h"

#include <limits.h>
#include <string.h>

#include "common.h"
#include "file/file.h"
#include "logger.h"
#include "sys/sys.h"
#include "utility/tlv.h"
#include "utility/utility.h"

#define SETTINGS_FILE_MAX_SIZE   4096u
#define SETTINGS_FILE_HEADER_LEN 4u
#define LEGACY_INT_SIZE          4u

static Storage __storage;

typedef struct {
    const DataDescriptor* dsc;
    size_t                count;
} StorageCtx;

static bool isNumericType(DataType type) {
    return type == T_U8 || type == T_U16 || type == T_U32 || type == T_U64 ||
           type == T_BOOL;
}

static uint16_t numericWidth(DataType type) {
    switch (type) {
    case T_U8:
        return (uint16_t)sizeof(uint8_t);
    case T_BOOL:
        return (uint16_t)sizeof(bool);
    case T_U16:
        return (uint16_t)sizeof(uint16_t);
    case T_U32:
        return (uint16_t)sizeof(uint32_t);
    case T_U64:
        return (uint16_t)sizeof(uint64_t);
    default:
        return 0u;
    }
}

static bool parseU64(const char* text, uint64_t* value) {
    uint64_t result = 0u;

    if (text == NULL || value == NULL || *text == '\0')
        return false;

    while (*text != '\0') {
        uint8_t digit;

        if (*text < '0' || *text > '9')
            return false;

        digit = (uint8_t)(*text - '0');
        if (result > (UINT64_MAX - digit) / 10u)
            return false;

        result = result * 10u + digit;
        text++;
    }

    *value = result;
    return true;
}

static bool numericValueFits(DataType type, uint64_t value) {
    switch (type) {
    case T_U8:
        return value <= UINT8_MAX;
    case T_U16:
        return value <= UINT16_MAX;
    case T_U32:
        return value <= UINT32_MAX;
    case T_U64:
        return true;
    case T_BOOL:
        return value <= 1u;
    default:
        return false;
    }
}

static void writeNumeric(void* address, DataType type, uint64_t value) {
    switch (type) {
    case T_U8: {
        uint8_t v = (uint8_t)value;
        memcpy(address, &v, sizeof(v));
        break;
    }
    case T_U16: {
        uint16_t v = (uint16_t)value;
        memcpy(address, &v, sizeof(v));
        break;
    }
    case T_U32: {
        uint32_t v = (uint32_t)value;
        memcpy(address, &v, sizeof(v));
        break;
    }
    case T_U64: {
        uint64_t v = value;
        memcpy(address, &v, sizeof(v));
        break;
    }
    case T_BOOL: {
        bool v = value != 0u;
        memcpy(address, &v, sizeof(v));
        break;
    }
    default:
        break;
    }
}

static bool readNumericItem(const TlvItem*        item,
                            const DataDescriptor* descriptor, uint64_t* value) {
    uint16_t expected = numericWidth(descriptor->type);

    if (item->valueLen == expected) {
        uint64_t result = 0u;
        memcpy(&result, item->value, expected);
        *value = result;
        return numericValueFits(descriptor->type, result);
    }

    /*
     * Old firmware copied four bytes starting at every T_INT address. For a
     * one- or two-byte packed field, the trailing bytes belonged to adjacent
     * fields. Preserve only this field's leading bytes rather than treating
     * the complete, contaminated four-byte block as its value.
     */
    if (item->valueLen == LEGACY_INT_SIZE && expected != LEGACY_INT_SIZE) {
        uint64_t legacy = 0u;
        uint16_t copyLen =
            expected < LEGACY_INT_SIZE ? expected : LEGACY_INT_SIZE;
        memcpy(&legacy, item->value, copyLen);
        *value = legacy;
        return numericValueFits(descriptor->type, legacy);
    }

    return false;
}

static Error_t storageTlvHandler(const TlvItem* item, void* user) {
    StorageCtx* ctx;

    RETURN_VALUE_IF_NULL(item, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(user, ;, ERR_NULL_PARAMETER);
    ctx = (StorageCtx*)user;

    for (size_t i = 0u; i < ctx->count; i++) {
        const DataDescriptor* descriptor = &ctx->dsc[i];
        size_t                keyLen     = strlen(descriptor->key);

        if (keyLen != item->tagLen ||
            memcmp(descriptor->key, item->tag, item->tagLen) != 0) {
            continue;
        }

        if (isNumericType(descriptor->type)) {
            uint64_t value = 0u;

            if (!readNumericItem(item, descriptor, &value)) {
                LOG_ERROR("Ignoring invalid setting '%s' (length=%u)",
                          descriptor->key, item->valueLen);
                return ERR_OK;
            }

            writeNumeric(descriptor->address, descriptor->type, value);
            return ERR_OK;
        }

        if (descriptor->type == T_BINARY) {
            if (item->valueLen < descriptor->minLen ||
                item->valueLen > descriptor->maxLen) {
                LOG_ERROR("Ignoring invalid binary setting '%s' (length=%u)",
                          descriptor->key, item->valueLen);
                return ERR_OK;
            }

            memset(descriptor->address, 0, descriptor->maxLen);
            memcpy(descriptor->address, item->value, item->valueLen);
            return ERR_OK;
        }

        if (descriptor->type == T_STRING) {
            if (item->valueLen < descriptor->minLen ||
                item->valueLen > descriptor->maxLen) {
                LOG_ERROR("Ignoring invalid string setting '%s' (length=%u)",
                          descriptor->key, item->valueLen);
                return ERR_OK;
            }

            memset(descriptor->address, 0, descriptor->maxLen + 1u);
            memcpy(descriptor->address, item->value, item->valueLen);
            return ERR_OK;
        }

        LOG_ERROR("Ignoring setting '%s' with unknown type", descriptor->key);
        return ERR_OK;
    }

    /* Unknown keys belong to a different schema version; safely ignore. */
    return ERR_OK;
}

static void initStorage(const DataDescriptor* dsc, size_t itemsCount) {
    if (dsc == NULL)
        return;

    for (size_t i = 0u; i < itemsCount; i++) {
        const DataDescriptor* descriptor = &dsc[i];

        if (isNumericType(descriptor->type)) {
            uint64_t defaultValue = 0u;

            if (descriptor->defaultValue != NULL &&
                !parseU64(descriptor->defaultValue, &defaultValue)) {
                LOG_ERROR("Invalid numeric default for '%s'", descriptor->key);
                defaultValue = 0u;
            }

            if (!numericValueFits(descriptor->type, defaultValue)) {
                LOG_ERROR("Numeric default out of range for '%s'",
                          descriptor->key);
                defaultValue = 0u;
            }

            writeNumeric(descriptor->address, descriptor->type, defaultValue);
            continue;
        }

        if (descriptor->type == T_BINARY) {
            memset(descriptor->address, 0, descriptor->maxLen);
            if (descriptor->defaultValue != NULL) {
                size_t len = strlen(descriptor->defaultValue);
                if (len > descriptor->maxLen)
                    len = descriptor->maxLen;
                memcpy(descriptor->address, descriptor->defaultValue, len);
            }
            continue;
        }

        if (descriptor->type == T_STRING) {
            memset(descriptor->address, 0, descriptor->maxLen + 1u);
            if (descriptor->defaultValue != NULL) {
                size_t len = strlen(descriptor->defaultValue);
                if (len > descriptor->maxLen)
                    len = descriptor->maxLen;
                memcpy(descriptor->address, descriptor->defaultValue, len);
            }
        }
    }
}

static bool isSettingsFileCorrect(const uint8_t* settingsData,
                                  uint32_t       settingsDataLen,
                                  uint16_t       expectedCrc) {
    RETURN_VALUE_IF_NULL(settingsData, ;, false);
    return (uint16_t)libCrc16((uint8_t*)settingsData, settingsDataLen) ==
           expectedCrc;
}

static Error_t readFileHeadInfo(const char* addr, uint16_t* fileSize,
                                uint16_t* fileCrc) {
    uint8_t     header[SETTINGS_FILE_HEADER_LEN] = {0};
    FileHandle* fp;
    size_t      readSize;

    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(fileSize, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(fileCrc, ;, ERR_NULL_PARAMETER);

    fp = OOP_CALL(file(), open, addr, "rb");
    if (fp == NULL) {
        LOG_INFO("Storage file does not exist: %s", addr);
        return ERR_NOK;
    }

    /*
     * Some embedded file implementations do not guarantee that the initial
     * position is zero, even after opening with "rb".
     */
    if (OOP_CALL(file(), seek, fp, 0, FILE_SEEK_ORG_SET) != 0) {
        LOG_ERROR("Cannot seek storage file: %s", addr);
        OOP_CALL(file(), close, fp);
        return ERR_NOK;
    }

    readSize = OOP_CALL(file(), read, header, sizeof(header), 1u, fp);

    OOP_CALL(file(), close, fp);

    if (readSize != sizeof(header)) {
        LOG_INFO("Storage file is empty/truncated: %s, read=%u expected=%u",
                 addr, (unsigned)readSize, (unsigned)sizeof(header));
        return ERR_NOK;
    }

    *fileSize = (uint16_t)(((uint16_t)header[0] << 8u) | header[1]);

    *fileCrc = (uint16_t)(((uint16_t)header[2] << 8u) | header[3]);

    if (*fileSize == 0u ||
        *fileSize > SETTINGS_FILE_MAX_SIZE - SETTINGS_FILE_HEADER_LEN) {
        LOG_ERROR("Invalid storage payload size: %s, size=%u", addr, *fileSize);
        return ERR_NOK;
    }

    return ERR_OK;
}

static uint16_t itemWriteLength(const DataDescriptor* descriptor) {
    if (isNumericType(descriptor->type))
        return numericWidth(descriptor->type);

    if (descriptor->type == T_BINARY)
        return descriptor->maxLen;

    if (descriptor->type == T_STRING) {
        size_t len = strlen((const char*)descriptor->address);
        return (uint16_t)(len > descriptor->maxLen ? descriptor->maxLen : len);
    }

    return 0u;
}

static Error_t saveStorage(const DataDescriptor* dsc, size_t itemsCount,
                           const char* addr) {
    uint8_t*    fileWriteBuf;
    uint32_t    writeBufLen = SETTINGS_FILE_HEADER_LEN;
    uint16_t    crc16;
    uint16_t    dataLen;
    FileHandle* fp;
    int32_t     writtenToFile;

    RETURN_VALUE_IF_NULL(dsc, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);

    if (*addr == '\0')
        return ERR_BAD_PARAMETER;

    fileWriteBuf = (uint8_t*)MEM_ALLOC(SETTINGS_FILE_MAX_SIZE);
    if (fileWriteBuf == NULL)
        return ERR_NOK;

    memset(fileWriteBuf, 0, SETTINGS_FILE_MAX_SIZE);

    for (size_t i = 0u; i < itemsCount; i++) {
        uint16_t   itemLen = itemWriteLength(&dsc[i]);
        uint16_t   encoded = 0u;
        uint32_t   remaining;
        TlvError_t err;

        if (itemLen == 0u && dsc[i].type != T_STRING) {
            LOG_ERROR("Invalid descriptor '%s'", dsc[i].key);
            MEM_FREE(fileWriteBuf);
            return ERR_NOK;
        }

        remaining = SETTINGS_FILE_MAX_SIZE - writeBufLen;
        err = tlv()->encode(fileWriteBuf + writeBufLen, remaining,
                            (uint8_t*)dsc[i].key, (uint16_t)strlen(dsc[i].key),
                            (uint8_t*)dsc[i].address, itemLen, &encoded);

        if (err != TLV_OK || encoded > remaining) {
            LOG_ERROR("Cannot encode setting '%s': %d", dsc[i].key, err);
            MEM_FREE(fileWriteBuf);
            return ERR_NOK;
        }

        writeBufLen += encoded;
    }

    dataLen = (uint16_t)(writeBufLen - SETTINGS_FILE_HEADER_LEN);
    crc16 =
        (uint16_t)libCrc16(fileWriteBuf + SETTINGS_FILE_HEADER_LEN, dataLen);

    fileWriteBuf[0] = (uint8_t)(dataLen >> 8u);
    fileWriteBuf[1] = (uint8_t)(dataLen & 0xFFu);
    fileWriteBuf[2] = (uint8_t)(crc16 >> 8u);
    fileWriteBuf[3] = (uint8_t)(crc16 & 0xFFu);

    fp = OOP_CALL(file(), open, addr, "wb");
    if (fp == NULL) {
        MEM_FREE(fileWriteBuf);
        return ERR_NOK;
    }

    writtenToFile =
        OOP_CALL(file(), overwrite, fileWriteBuf, writeBufLen, 1u, fp);
    OOP_CALL(file(), close, fp);
    MEM_FREE(fileWriteBuf);

    if (writtenToFile != (int32_t)writeBufLen) {
        LOG_ERROR("Incomplete settings write: %d/%u", writtenToFile,
                  (unsigned)writeBufLen);
        return ERR_NOK;
    }

    return ERR_OK;
}

static Error_t loadStorage(const DataDescriptor* dsc, size_t itemsCount,
                           const char* addr) {
    uint8_t*    fileCache = NULL;
    uint16_t    fileSize  = 0u;
    uint16_t    fileCrc   = 0u;
    FileHandle* fp        = NULL;
    size_t      readSize;
    TlvError_t  decodeError;
    StorageCtx  ctx;

    RETURN_VALUE_IF_NULL(dsc, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);

    if (*addr == '\0')
        return ERR_BAD_PARAMETER;

    /* Current-schema defaults first; the file is only an overlay. */
    initStorage(dsc, itemsCount);

    Error_t headerResult = readFileHeadInfo(addr, &fileSize, &fileCrc);

    if (headerResult != ERR_OK) {
        Error_t saveResult;

        LOG_INFO("Initializing storage file with defaults: %s", addr);

        saveResult = saveStorage(dsc, itemsCount, addr);

        if (saveResult != ERR_OK) {
            LOG_ERROR("Failed to initialize storage file: %s, error=%d", addr,
                      saveResult);
        }

        return saveResult;
    }

    fileCache = (uint8_t*)MEM_ALLOC(fileSize);
    if (fileCache == NULL)
        return ERR_NOK;

    fp = OOP_CALL(file(), open, addr, "rb");
    if (fp == NULL)
        goto corrupted_file;

    if (OOP_CALL(file(), seek, fp, SETTINGS_FILE_HEADER_LEN,
                 FILE_SEEK_ORG_SET) != 0) {
        goto corrupted_file;
    }

    readSize = OOP_CALL(file(), read, fileCache, fileSize, 1u, fp);
    OOP_CALL(file(), close, fp);
    fp = NULL;

    if (readSize != fileSize ||
        !isSettingsFileCorrect(fileCache, fileSize, fileCrc)) {
        goto corrupted_file;
    }

    ctx.dsc     = dsc;
    ctx.count   = itemsCount;
    decodeError = tlv()->decode(fileCache, fileSize, storageTlvHandler, &ctx);
    if (decodeError != TLV_OK)
        goto corrupted_file;

    MEM_FREE(fileCache);

    /* Missing keys remain at their defaults without an unnecessary write.
     */
    return ERR_OK;

corrupted_file:
    if (fp != NULL)
        OOP_CALL(file(), close, fp);
    MEM_FREE(fileCache);

    /* Decoding may have applied a prefix; restore a clean default state. */
    initStorage(dsc, itemsCount);
    return saveStorage(dsc, itemsCount, addr);
}

static Error_t resetStorage(const DataDescriptor* dsc, size_t itemsCount,
                            const char* addr) {
    RETURN_VALUE_IF_NULL(dsc, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);
    initStorage(dsc, itemsCount);
    return saveStorage(dsc, itemsCount, addr);
}

OOP_CTOR(Storage) {
    self->load  = loadStorage;
    self->save  = saveStorage;
    self->reset = resetStorage;
}

Storage* storage(void) {
    CALL_ONCE(OOP_CALL_CTOR(Storage, &__storage););
    return &__storage;
}
