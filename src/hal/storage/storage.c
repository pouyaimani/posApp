#include "storage.h"
#include "dev/dev.h"
#include "file/file.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"
#include "utility/tlv.h"

#define SETTINGS_FILE_MAX_SIZE  4096
#define SETTINGS_FILE_HEADER_LEN    4

static Storage __storage;

typedef struct {
    DataDescriptor *dsc;
    uint32_t count;
} StorageCtx;

int storageTlvHandler(const TlvItem *item, void *user) {
    StorageCtx *ctx = (StorageCtx *)user;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        DataDescriptor *s = &ctx->dsc[i];

        if (strlen(s->key) != item->tagLen)
            continue;

        if (memcmp(s->key, item->tag, item->tagLen) != 0)
            continue;

        // --- VALIDATION ---
        if ((s->type) == T_INT) {
            if (item->valueLen != sizeof(uint32_t)) {
                LOG_TRACE("item %d valueLen = %d, expcted len = %d", i, item->valueLen, sizeof(uint32_t));
                return -1;
            }
        } else if ((s->type) == T_BYTE) {
            if (item->valueLen != sizeof(uint8_t))  {
                LOG_TRACE("item %d valueLen = %d, expcted len = %d", i, item->valueLen, sizeof(uint8_t));
                return -1;
            }
        } else if (s->type == T_BINARY) {
            if (item->valueLen != s->maxLen) {
                LOG_TRACE("binary size mismatch");
                return -1;
            }
        } else {
            if (item->valueLen > s->maxLen ||
                item->valueLen < s->minLen) {
                    LOG_TRACE("item %d valueLen = %d, max len = %d, min len = %d", i, item->valueLen, s->maxLen, s->minLen);
                    return -1;
                }
            memset(s->address, 0, s->maxLen + 1);
        }

        memcpy(s->address, item->value, item->valueLen);
        break;
    }

    return 0;
}

static void init(DataDescriptor *dsc, uint32_t itemsCount) {
    uint32_t i = 0;
    LOG_TRACE("itemsCount = %d", itemsCount);

    for (i = 0; i < itemsCount; i++)
    {
        if ((dsc[i].type) == T_INT) {
            uint32_t intDefaultValue = (uint32_t) libAtoi(dsc[i].defaultValue);
            memcpy(dsc[i].address, &intDefaultValue, sizeof(uint32_t));
            LOG_TRACE("%d.%20s:%d", i, dsc[i].key, intDefaultValue);
        } else if ((dsc[i].type) == T_BYTE) {
            uint8_t byteDefaultValue = (uint8_t) libAtoi(dsc[i].defaultValue);
            memcpy(dsc[i].address, &byteDefaultValue, sizeof(uint8_t));
            LOG_TRACE("%d.%20s:%d", i, dsc[i].key, byteDefaultValue);
        } else if (dsc[i].type == T_BINARY) {
            memset(dsc[i].address, 0 , dsc[i].maxLen);
        } else {
            memset(dsc[i].address, 0, dsc[i].maxLen + 1);
            if (dsc[i].defaultValue != NULL) {
                memcpy(dsc[i].address, dsc[i].defaultValue, strlen((const char *) dsc[i].defaultValue));
                LOG_TRACE("%d.%20s:%s", i, dsc[i].key, dsc[i].defaultValue);
            }
        }
    }
}

static bool isSettingsFileCorrect(uint8_t *settingsData, uint32_t settingsDataLen, uint32_t crc) {
    uint16_t calcCrc = 0;

    calcCrc = (uint16_t) libCrc16(settingsData, settingsDataLen);

    return calcCrc == crc ? true : false;
}

static int32_t readSettingsFileHeadInfo(char *addr, uint32_t *fileSize, uint32_t *fileCrc) {
    uint32_t readSize = SETTINGS_FILE_HEADER_LEN;
    uint8_t fileSizeBytes[SETTINGS_FILE_HEADER_LEN] = {0};
    int32_t ret = 0;
    FileHandle *fp = OOP_CALL(file(), open, addr, "rb");
    if (!fp) {
        return -1;
    }
    LOG_DEBUG("--------------------");
    if (OOP_CALL(file(), seek, fp, 0 , FILE_SEEK_ORG_SET) != 0) {
        OOP_CALL(file(), close, fp);
        return -1;
    }
    LOG_DEBUG("--------------------");
    size_t readsize = OOP_CALL(file(), read, fileSizeBytes, 4 , 1, fp);
    if (readsize != SETTINGS_FILE_HEADER_LEN)
    {
        LOG_ERROR("File read error. file read error = %d, read size = %d, "
                    "setting file header len = %d", ret, readsize, SETTINGS_FILE_HEADER_LEN);
        OOP_CALL(file(), close, fp);
        return -1;
    }
    LOG_DEBUG("--------------------");
    *fileSize = (uint32_t) (fileSizeBytes[0] * 256 + fileSizeBytes[1]);

    if (*fileSize > SETTINGS_FILE_MAX_SIZE || *fileSize == 0)
    {
        LOG_ERROR("File read error. file size = %d, max file size = %d",
                 fileSize, SETTINGS_FILE_MAX_SIZE);
        OOP_CALL(file(), close, fp);
        return -1;
    }
LOG_DEBUG("--------------------");
    *fileCrc = (uint32_t) (fileSizeBytes[2] * 256 + fileSizeBytes[3]);
    OOP_CALL(file(), close, fp);
    return 0;
}

static DataDescriptor *getDataDescriptor(char *name, DataDescriptor *dsc, uint32_t itemCount) {
    int32_t i = 0;
    DataDescriptor *item = NULL;

    for (i = 0; i < itemCount; i++)
    {
        if (0 == strcmp(name, dsc[i].key))
        {
            item = &dsc[i];
            break;
        }
    }

    return item;
}

static void saveStorage(DataDescriptor *dsc, size_t itemsCount, const char *addr) {
    uint8_t *fileWriteBuf = NULL;
    uint32_t itemWriteLen = 0;
    uint16_t crc16 = 0;
    uint32_t i = 0;
    uint16_t writeBufLen = 0;
    int32_t ret = 0;

    if (addr == NULL || strlen(addr) == 0)
    {
        LOG_ERROR("Address format err ...");
        return;
    }

    writeBufLen += SETTINGS_FILE_HEADER_LEN;

    fileWriteBuf = (uint8_t *) GET_MEM(SETTINGS_FILE_MAX_SIZE);
    if (fileWriteBuf == NULL)
    {
        return;
    }

    memset(fileWriteBuf, 0, SETTINGS_FILE_MAX_SIZE);
    for (i = 0; i < itemsCount; i++)
    {
        if ((dsc[i].type) == T_INT) {
            itemWriteLen = sizeof(uint32_t);
        } else if ((dsc[i].type) == T_BYTE) {
            itemWriteLen = sizeof(uint8_t);
        } else if (dsc[i].type == T_BINARY) {
            itemWriteLen = dsc[i].maxLen;
        } else {
            uint16_t strLen = strlen((char*)dsc[i].address);
            itemWriteLen = strLen > dsc[i].maxLen ? dsc[i].maxLen : strLen;
        }
        int remainedSize = SETTINGS_FILE_MAX_SIZE - writeBufLen;
        uint16_t writen;
        TlvError_t err = tlv()->encode(fileWriteBuf + writeBufLen, remainedSize, (uint8_t *) dsc[i].key, 
            strlen(dsc[i].key), 
                (uint8_t *) dsc[i].address, (uint16_t) itemWriteLen, &writen);  
        writeBufLen += writen;
        if (writeBufLen >= SETTINGS_FILE_MAX_SIZE || err != TLV_OK) {
            LOG_ERROR("Error in saving storage. tlv decoder error = %d, "
                        "wirte buffer len = %d, max file size = %d", err, writeBufLen, SETTINGS_FILE_MAX_SIZE);
            break;
        }
    }

    crc16 = (uint16_t) libCrc16(fileWriteBuf + SETTINGS_FILE_HEADER_LEN, (uint32_t) (writeBufLen - SETTINGS_FILE_HEADER_LEN));
    uint16_t dataLen = writeBufLen - SETTINGS_FILE_HEADER_LEN;

    fileWriteBuf[0] = (uint8_t)(dataLen / 256);
    fileWriteBuf[1] = (uint8_t)(dataLen % 256);
    fileWriteBuf[2] = (uint8_t) (crc16 >> 8u);
    fileWriteBuf[3] = (uint8_t) (crc16 & 0xFFu);
    FileHandle *fp = OOP_CALL(file(), open, addr, "wb");
    if (!fp) {
        return -1;
    }
    if (OOP_CALL(file(), seek, fp, 0 , FILE_SEEK_ORG_SET) != 0) {
        OOP_CALL(file(), close, fp);
        return -1;
    }
    ret = OOP_CALL(file(), write, fileWriteBuf, writeBufLen, 1, fp);
    OOP_CALL(file(), close, fp);
    if (ret < 0) {
        return -1;
    }
    LOG_TRACE("Saving storage is successfully done.");
    FREE_MEM(fileWriteBuf);
}

static void loadStorage(DataDescriptor *dsc, size_t itemsCount, const char *addr) {
    uint8_t *fileCaches = NULL;
    uint32_t fileSize = 0;
    uint32_t fileCrc = 0;
    uint32_t readSize = 0;
    int32_t ret = -1;
    // goto init_settings;

    if (addr == NULL || strlen(addr) == 0)
    {
        LOG_ERROR("reloadSettings err , addr is err format or func is null...");
        return;
    }

    ret = readSettingsFileHeadInfo(addr, &fileSize, &fileCrc);
    if (ret != 0)
    {
        LOG_ERROR("read file head info err ret = %d filesize:%d", ret, fileSize);
        goto init_settings;
    }

    fileCaches = (uint8_t *) GET_MEM(fileSize);
    if (fileCaches == NULL) {
        LOG_ERROR("Error in reading storage.");
        goto init_settings;
    }
    memset(fileCaches, 0, fileSize);
    readSize = fileSize;
    FileHandle *fp = OOP_CALL(file(), open, addr, "rb");
    if (!fp) {
        return -1;
    }
    if (OOP_CALL(file(), seek, fp, SETTINGS_FILE_HEADER_LEN , FILE_SEEK_ORG_SET) != 0) {
        OOP_CALL(file(), close, fp);
        return -1;
    }
    readSize = OOP_CALL(file(), read, fileCaches, fileSize, 1, fp);
    if (readSize != fileSize) {
        LOG_ERROR("Error in reading storage. error = %d, read sise = %d, "
                        "file size = %d", ret, readSize, fileSize);
        OOP_CALL(file(), close, fp);
        goto init_settings;
    }
    OOP_CALL(file(), close, fp);
    if (!isSettingsFileCorrect(fileCaches, fileSize, fileCrc))
    {
        LOG_ERROR("error in file verifying.");
        goto init_settings;
    }

    StorageCtx ctx = {
        .dsc = dsc,
        .count = itemsCount
    };
    TlvError_t err = tlv()->decode(fileCaches, fileSize, storageTlvHandler, &ctx);
    if (err != TLV_OK) {
        LOG_ERROR("Error in parsing storage tlv. error = %d", err);
        goto init_settings;
    }

    LOG_TRACE("parse settings tlv data success ... ");
    FREE_MEM(fileCaches);
    return;

    init_settings:

    if (fileCaches != NULL) FREE_MEM(fileCaches);

    init(dsc, itemsCount);
    saveStorage(dsc, itemsCount, addr);
}

static void resetStorage(DataDescriptor *dsc, size_t itemsCount, const char *addr) {
    init(dsc, itemsCount);
    saveStorage(dsc, itemsCount, addr);
}

OOP_CTOR(Storage) {
    self->load = loadStorage;
    self->save = saveStorage;
    self->reset= resetStorage;
}

Storage *storage() {
    CALL_ONCE(
        OOP_CALL_CTOR(Storage, &__storage);
    );
    return &__storage;
}