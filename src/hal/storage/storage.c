#include "storage.h"

#include <limits.h>
#include <string.h>

#include "common.h"
#include "file/file.h"
#include "logger.h"
#include "sys/sys.h"
#include "utility/tlv.h"
#include "utility/utility.h"

/* T3 file I/O is unreliable above this limit. */
#define SETTINGS_FILE_MAX_SIZE   1024u
#define SETTINGS_FILE_HEADER_LEN 4u
#define STORAGE_TAG_LEN          2u
#define LEGACY_INT_SIZE          4u

static Storage __storage;

typedef struct {
    const DataDescriptor* dsc;
    size_t                count;
    size_t                matchedCount;
    bool                  needsRewrite;
} StorageCtx;

static size_t boundedStringLength(const char* text, size_t maxLen) {
    size_t len = 0u;

    if (text == NULL)
        return 0u;

    while (len < maxLen && text[len] != '\0')
        len++;

    return len;
}

static bool isNumericType(DataType type) {
    return type == T_U8 || type == T_U16 || type == T_U32 || type == T_U64 ||
           type == T_BOOL;
}

static uint16_t numericWidth(DataType type) {
    switch (type) {
    case T_U8:
        return (uint16_t)sizeof(uint8_t);
    case T_U16:
        return (uint16_t)sizeof(uint16_t);
    case T_U32:
        return (uint16_t)sizeof(uint32_t);
    case T_U64:
        return (uint16_t)sizeof(uint64_t);
    case T_BOOL:
        return (uint16_t)sizeof(bool);
    default:
        return 0u;
    }
}

static bool descriptorTableIsValid(const DataDescriptor* dsc,
                                   size_t                itemsCount) {
    if (dsc == NULL || itemsCount == 0u)
        return false;

    for (size_t i = 0u; i < itemsCount; i++) {
        const DataDescriptor* descriptor = &dsc[i];

        if (descriptor->id == 0u || descriptor->id == UINT16_MAX ||
            descriptor->address == NULL) {
            LOG_ERROR("Invalid storage descriptor: index=%u id=%u", (unsigned)i,
                      descriptor->id);
            return false;
        }

        if (isNumericType(descriptor->type)) {
            uint16_t expected = numericWidth(descriptor->type);

            if (expected == 0u || descriptor->minLen != expected ||
                descriptor->maxLen != expected) {
                LOG_ERROR("Invalid numeric descriptor: id=%u", descriptor->id);
                return false;
            }
        } else if (descriptor->type == T_STRING) {
            if (descriptor->maxLen == 0u) {
                LOG_ERROR("Invalid string descriptor: id=%u", descriptor->id);
                return false;
            }
        } else if (descriptor->type == T_BINARY) {
            if (descriptor->maxLen == 0u ||
                descriptor->minLen > descriptor->maxLen) {
                LOG_ERROR("Invalid binary descriptor: id=%u", descriptor->id);
                return false;
            }
        } else {
            LOG_ERROR("Unknown descriptor type: id=%u", descriptor->id);
            return false;
        }

        for (size_t j = i + 1u; j < itemsCount; j++) {
            if (descriptor->id == dsc[j].id) {
                LOG_ERROR("Duplicate storage ID: %u", descriptor->id);
                return false;
            }
        }
    }

    return true;
}

static bool descriptorMatches(const DataDescriptor* descriptor,
                              const TlvItem*        item) {
    uint16_t id;

    if (item->tagLen != STORAGE_TAG_LEN)
        return false;

    id = (uint16_t)(((uint16_t)item->tag[0] << 8u) | (uint16_t)item->tag[1]);
    return id == descriptor->id;
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
        uint8_t converted = (uint8_t)value;
        memcpy(address, &converted, sizeof(converted));
        break;
    }
    case T_U16: {
        uint16_t converted = (uint16_t)value;
        memcpy(address, &converted, sizeof(converted));
        break;
    }
    case T_U32: {
        uint32_t converted = (uint32_t)value;
        memcpy(address, &converted, sizeof(converted));
        break;
    }
    case T_U64: {
        uint64_t converted = value;
        memcpy(address, &converted, sizeof(converted));
        break;
    }
    case T_BOOL: {
        bool converted = value != 0u;
        memcpy(address, &converted, sizeof(converted));
        break;
    }
    default:
        break;
    }
}

static bool readNumericItem(const TlvItem*        item,
                            const DataDescriptor* descriptor, uint64_t* value,
                            bool* usedLegacyWidth) {
    uint16_t expected = numericWidth(descriptor->type);

    *usedLegacyWidth = false;

    if (item->valueLen == expected) {
        uint64_t decoded = 0u;
        memcpy(&decoded, item->value, expected);
        *value = decoded;
        return numericValueFits(descriptor->type, decoded);
    }

    /*
     * The previous format wrote four bytes for every T_INT, including packed
     * one- and two-byte members. Only the leading bytes belong to the field.
     * The deployed terminals use the same native little-endian representation.
     */
    if (item->valueLen == LEGACY_INT_SIZE && expected != LEGACY_INT_SIZE) {
        uint64_t decoded = 0u;
        uint16_t copyLen =
            expected < LEGACY_INT_SIZE ? expected : LEGACY_INT_SIZE;

        memcpy(&decoded, item->value, copyLen);
        *value           = decoded;
        *usedLegacyWidth = true;
        return numericValueFits(descriptor->type, decoded);
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
        if (!descriptorMatches(descriptor, item))
            continue;

        ctx->matchedCount++;

        if (isNumericType(descriptor->type)) {
            uint64_t value           = 0u;
            bool     usedLegacyWidth = false;

            if (!readNumericItem(item, descriptor, &value, &usedLegacyWidth)) {
                LOG_ERROR("Ignoring invalid numeric setting: id=%u length=%u",
                          descriptor->id, item->valueLen);
                return ERR_OK;
            }

            if (usedLegacyWidth)
                ctx->needsRewrite = true;

            writeNumeric(descriptor->address, descriptor->type, value);
            return ERR_OK;
        }

        if (descriptor->type == T_BINARY) {
            if (item->valueLen < descriptor->minLen ||
                item->valueLen > descriptor->maxLen) {
                LOG_ERROR("Ignoring invalid binary setting: id=%u length=%u",
                          descriptor->id, item->valueLen);
                return ERR_OK;
            }

            memset(descriptor->address, 0, descriptor->maxLen);
            memcpy(descriptor->address, item->value, item->valueLen);
            return ERR_OK;
        }

        if (descriptor->type == T_STRING) {
            if (item->valueLen < descriptor->minLen ||
                item->valueLen > descriptor->maxLen) {
                LOG_ERROR("Ignoring invalid string setting: id=%u length=%u",
                          descriptor->id, item->valueLen);
                return ERR_OK;
            }

            memset(descriptor->address, 0, descriptor->maxLen + 1u);
            memcpy(descriptor->address, item->value, item->valueLen);
            return ERR_OK;
        }

        return ERR_OK;
    }

    /* Unknown IDs are intentionally ignored for forward compatibility. */
    return ERR_OK;
}

static void initStorage(const DataDescriptor* dsc, size_t itemsCount) {
    for (size_t i = 0u; i < itemsCount; i++) {
        const DataDescriptor* descriptor = &dsc[i];

        if (isNumericType(descriptor->type)) {
            uint64_t defaultValue = 0u;

            if (!parseU64(descriptor->defaultValue, &defaultValue) ||
                !numericValueFits(descriptor->type, defaultValue)) {
                LOG_ERROR("Invalid numeric default: id=%u", descriptor->id);
                defaultValue = 0u;
            }

            writeNumeric(descriptor->address, descriptor->type, defaultValue);
            continue;
        }

        if (descriptor->type == T_BINARY) {
            memset(descriptor->address, 0, descriptor->maxLen);
            if (descriptor->defaultValue != NULL) {
                size_t len = boundedStringLength(descriptor->defaultValue,
                                                 descriptor->maxLen);
                memcpy(descriptor->address, descriptor->defaultValue, len);
            }
            continue;
        }

        if (descriptor->type == T_STRING) {
            memset(descriptor->address, 0, descriptor->maxLen + 1u);
            if (descriptor->defaultValue != NULL) {
                size_t len = boundedStringLength(descriptor->defaultValue,
                                                 descriptor->maxLen);
                memcpy(descriptor->address, descriptor->defaultValue, len);
            }
        }
    }
}

static bool payloadCrcIsValid(const uint8_t* data, uint16_t dataLen,
                              uint16_t expectedCrc) {
    RETURN_VALUE_IF_NULL(data, ;, false);
    return (uint16_t)libCrc16((uint8_t*)data, dataLen) == expectedCrc;
}

static Error_t readFileHeadInfo(const char* addr, uint16_t* payloadSize,
                                uint16_t* payloadCrc) {
    uint8_t     header[SETTINGS_FILE_HEADER_LEN] = {0};
    FileHandle* fp;
    size_t      readSize;

    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(payloadSize, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(payloadCrc, ;, ERR_NULL_PARAMETER);

    fp = OOP_CALL(file(), open, addr, "r");
    if (fp == NULL) {
        LOG_INFO("Storage file does not exist: %s", addr);
        return ERR_NOK;
    }

    if (OOP_CALL(file(), seek, fp, 0, FILE_SEEK_ORG_SET) != 0) {
        LOG_ERROR("Cannot seek storage file: %s", addr);
        OOP_CALL(file(), close, fp);
        return ERR_NOK;
    }

    readSize = OOP_CALL(file(), read, header, 1u, sizeof(header), fp);
    OOP_CALL(file(), close, fp);

    if (readSize != sizeof(header)) {
        LOG_ERROR("Invalid storage header: path=%s read=%u expected=%u", addr,
                  (unsigned)readSize, (unsigned)sizeof(header));
        return ERR_NOK;
    }

    *payloadSize = (uint16_t)(((uint16_t)header[0] << 8u) | header[1]);
    *payloadCrc  = (uint16_t)(((uint16_t)header[2] << 8u) | header[3]);

    if (*payloadSize == 0u ||
        *payloadSize > SETTINGS_FILE_MAX_SIZE - SETTINGS_FILE_HEADER_LEN) {
        LOG_ERROR("Invalid storage payload size: path=%s size=%u", addr,
                  *payloadSize);
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
        return (uint16_t)boundedStringLength((const char*)descriptor->address,
                                             descriptor->maxLen);
    }

    return 0u;
}

static Error_t saveStorage(const DataDescriptor* dsc, size_t itemsCount,
                           const char* addr) {
    uint8_t*    fileBuffer;
    uint32_t    totalLen = SETTINGS_FILE_HEADER_LEN;
    uint16_t    payloadLen;
    uint16_t    crc;
    FileHandle* fp;
    size_t      written;

    RETURN_VALUE_IF_NULL(dsc, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);

    if (*addr == '\0' || !descriptorTableIsValid(dsc, itemsCount))
        return ERR_BAD_PARAMETER;

    fileBuffer = (uint8_t*)MEM_ALLOC(SETTINGS_FILE_MAX_SIZE);
    if (fileBuffer == NULL)
        return ERR_NOK;

    memset(fileBuffer, 0, SETTINGS_FILE_MAX_SIZE);

    for (size_t i = 0u; i < itemsCount; i++) {
        const DataDescriptor* descriptor = &dsc[i];
        uint16_t              itemLen    = itemWriteLength(descriptor);
        uint16_t              encodedLen = 0u;
        uint32_t              remaining;
        uint8_t               tag[STORAGE_TAG_LEN];
        TlvError_t            tlvError;

        if (totalLen >= SETTINGS_FILE_MAX_SIZE) {
            LOG_ERROR("Storage buffer full before id=%u", descriptor->id);
            MEM_FREE(fileBuffer);
            return ERR_NOK;
        }

        remaining = SETTINGS_FILE_MAX_SIZE - totalLen;
        tag[0]    = (uint8_t)(descriptor->id >> 8u);
        tag[1]    = (uint8_t)(descriptor->id & 0xFFu);

        tlvError =
            tlv()->encode(fileBuffer + totalLen, remaining, tag, sizeof(tag),
                          (uint8_t*)descriptor->address, itemLen, &encodedLen);

        if (tlvError != TLV_OK || encodedLen == 0u || encodedLen > remaining) {
            LOG_ERROR("Cannot encode setting: id=%u error=%d", descriptor->id,
                      tlvError);
            MEM_FREE(fileBuffer);
            return ERR_NOK;
        }

        totalLen += encodedLen;
    }

    payloadLen = (uint16_t)(totalLen - SETTINGS_FILE_HEADER_LEN);
    crc = (uint16_t)libCrc16(fileBuffer + SETTINGS_FILE_HEADER_LEN, payloadLen);

    fileBuffer[0] = (uint8_t)(payloadLen >> 8u);
    fileBuffer[1] = (uint8_t)(payloadLen & 0xFFu);
    fileBuffer[2] = (uint8_t)(crc >> 8u);
    fileBuffer[3] = (uint8_t)(crc & 0xFFu);

    fp = OOP_CALL(file(), open, addr, "w");
    if (fp == NULL) {
        LOG_ERROR("Cannot open storage for writing: %s", addr);
        MEM_FREE(fileBuffer);
        return ERR_NOK;
    }

    written = OOP_CALL(file(), overwrite, fileBuffer, 1u, totalLen, fp);
    OOP_CALL(file(), close, fp);
    MEM_FREE(fileBuffer);

    if (written != totalLen) {
        LOG_ERROR("Incomplete storage write: path=%s written=%u expected=%u",
                  addr, (unsigned)written, (unsigned)totalLen);
        return ERR_NOK;
    }

    LOG_TRACE("Storage saved: path=%s bytes=%u", addr, (unsigned)totalLen);
    return ERR_OK;
}

static Error_t loadStorage(const DataDescriptor* dsc, size_t itemsCount,
                           const char* addr) {
    uint8_t*    payload     = NULL;
    uint16_t    payloadSize = 0u;
    uint16_t    payloadCrc  = 0u;
    FileHandle* fp          = NULL;
    size_t      readSize;
    StorageCtx  ctx;
    TlvError_t  tlvError;

    RETURN_VALUE_IF_NULL(dsc, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);

    if (*addr == '\0' || !descriptorTableIsValid(dsc, itemsCount))
        return ERR_BAD_PARAMETER;

    /* An old file is an overlay on the complete current schema defaults. */
    initStorage(dsc, itemsCount);

    if (readFileHeadInfo(addr, &payloadSize, &payloadCrc) != ERR_OK)
        return saveStorage(dsc, itemsCount, addr);

    payload = (uint8_t*)MEM_ALLOC(payloadSize);
    if (payload == NULL)
        return ERR_NOK;

    fp = OOP_CALL(file(), open, addr, "r");
    if (fp == NULL)
        goto corrupted_file;

    if (OOP_CALL(file(), seek, fp, SETTINGS_FILE_HEADER_LEN,
                 FILE_SEEK_ORG_SET) != 0) {
        goto corrupted_file;
    }

    readSize = OOP_CALL(file(), read, payload, 1u, payloadSize, fp);
    OOP_CALL(file(), close, fp);
    fp = NULL;

    if (readSize != payloadSize ||
        !payloadCrcIsValid(payload, payloadSize, payloadCrc)) {
        LOG_ERROR("Storage read or CRC failure: %s", addr);
        goto corrupted_file;
    }

    ctx.dsc          = dsc;
    ctx.count        = itemsCount;
    ctx.matchedCount = 0u;
    ctx.needsRewrite = false;

    tlvError = tlv()->decode(payload, payloadSize, storageTlvHandler, &ctx);
    if (tlvError != TLV_OK) {
        LOG_ERROR("Storage TLV failure: path=%s error=%d", addr, tlvError);
        goto corrupted_file;
    }

    /* A string-tag or unrelated file must not silently load as all defaults. */
    if (ctx.matchedCount == 0u) {
        LOG_ERROR("Storage contains no recognized numeric IDs: %s", addr);
        goto corrupted_file;
    }

    MEM_FREE(payload);

    if (ctx.needsRewrite) {
        Error_t migrationResult = saveStorage(dsc, itemsCount, addr);

        if (migrationResult != ERR_OK) {
            /* Values are valid in RAM; migration can be retried next boot. */
            LOG_ERROR("Storage migration write failed: %s", addr);
        }
    }

    return ERR_OK;

corrupted_file:
    if (fp != NULL)
        OOP_CALL(file(), close, fp);
    MEM_FREE(payload);

    /* A partial decode must never leak into the active settings object. */
    initStorage(dsc, itemsCount);
    return saveStorage(dsc, itemsCount, addr);
}

static Error_t resetStorage(const DataDescriptor* dsc, size_t itemsCount,
                            const char* addr) {
    RETURN_VALUE_IF_NULL(dsc, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(addr, ;, ERR_NULL_PARAMETER);

    if (*addr == '\0' || !descriptorTableIsValid(dsc, itemsCount))
        return ERR_BAD_PARAMETER;

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
