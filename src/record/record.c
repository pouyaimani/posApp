#include "record.h"
#include "../dev/dev.h"
#include "eventloop.h"
#include "logger.h"
#include "file/file.h"
#include "utility/arith.h"

Record *__record;

typedef enum _RECORD_MATCH_TYPE {
    MATCH_INDEX,
    MATCH_VOUCHER,
    MATCH_TIME,
    MATCH_TRANS_STATUS,
} RECORD_MATCH_TYPE;

typedef enum {
    STATUS_UNKNOWN = 0,
    STATUS_ERROR = 2,
    STATUS_OK = 4,
} TRANS_STATUS;

#define RECORD_MAX_SIZE 6000
#define SUPPORT_CYCLE_REPLACE_RECORD_FULL true

#define USER_DATA_ROOT_DIR          "/mtd0/"

#define APP_DIR                     USER_DATA_ROOT_DIR
#define DEVICE_PROP_FILE            APP_DIR"lv_device_prop"

#define TRANS_RECORD_PATH   APP_DIR "app_data"

typedef enum
{
    CONVERT_INDEX_TYPE_TO_REAL = 0x01,   
    CONVERT_INDEX_TYPE_TO_ORDER = 0x02 
} CONVERT_INDEX_TYPE;

#define FILE_RECORD_INDEX          TRANS_RECORD_PATH   
#define FILE_RECORD                FILE_RECORD_INDEX  
#define FILE_RECORD_CACHE          FILE_RECORD_INDEX  
#define FILE_RECORD_LAST_INDEX     FILE_RECORD_INDEX  

#define OFFSET_RECORD_TOTAL        0                  
#define OFFSET_RECORD_LAST_INDEX   2                  
#define OFFSET_RECORD_CACHE        4                  
#define OFFSET_RECORD_INDEX        8192               
#define OFFSET_RECORD              (256 * 1024)       

#define SIZE_RECORD                    gRecordSize                                
#define SIZE_RECORD_INDEX              gRecordIndexSize                           
#define RECORD_MAX_SIZE                gRecordMaxNumber                           
#define SIZE_CRC                       2                                         
#define SIZE_RECORD_LAST_INDEX         2                                          
#define SIZE_RECORD_TOTAL              SIZE_RECORD_LAST_INDEX                     

#define SIZE_RECORD_HEAD               0                                            
#define SIZE_RECORD_ITEM_PREFIX        SIZE_CRC                                     
#define SIZE_RECORD_ITEM               (SIZE_RECORD_ITEM_PREFIX + SIZE_RECORD)      
#define SIZE_RECORD_CACHE              SIZE_RECORD                                  
#define SIZE_RECORD_INDEX_HEAD         0                                            
#define SIZE_RECORD_INDEX_ITEM_PREFIX  SIZE_CRC                                     
#define SIZE_RECORD_INDEX_ITEM         (SIZE_RECORD_INDEX_ITEM_PREFIX + SIZE_RECORD_INDEX)

/**
 * Get index data from transaction record data
 * @param record[in] Transaction data
 * @param recordIndex[out] Index data
 * @return ERR_OK on success, others on failure
 */
typedef int32_t (*GetRecordIndexFromRecordFunc)(uint8_t *record, uint8_t *recordIndex);

typedef bool (*MatchRecordIndexFunc)(uint32_t matchType, void *matchValue, uint8_t *recordIndex);

typedef enum {
    INDEX_NUM_TYPE_ONE,
    INDEX_NUM_TYPE_MORE
} INDEX_NUM_TYPE;

typedef struct _RecordMemory {
    uint8_t *memory;
    int32_t startIndex;
    int32_t endIndex;
} RecordMemory;

static uint32_t gRecordSize = 0;
static uint32_t gRecordIndexSize = 0;
static uint32_t gRecordMaxNumber = 0;

static GetRecordIndexFromRecordFunc gGetRecordIndexFromRecordFunc;

static uint32_t getRecordOffset(uint32_t index) {
    return OFFSET_RECORD + SIZE_RECORD_HEAD + (index * SIZE_RECORD_ITEM);
}

static uint32_t getRecordIndexPosition(uint32_t index) {
    return OFFSET_RECORD_INDEX + SIZE_RECORD_INDEX_HEAD + (index * SIZE_RECORD_INDEX_ITEM);
}

static int32_t getLastRecordIndex() {
    uint8_t lastIndexBytes[SIZE_RECORD_LAST_INDEX] = {0};
    uint32_t readLen = 0;

    readLen = sizeof(lastIndexBytes);
    if (OOP_CALL(file(), read, FILE_RECORD_LAST_INDEX, lastIndexBytes,
            OFFSET_RECORD_LAST_INDEX, &readLen) != FILE_ERR_OK) {
        return ERR_NOK;
    }
    hexToU32(&readLen, lastIndexBytes, sizeof(lastIndexBytes));
    return readLen;
}

static int32_t writeItem(s8 *fileName, uint32_t offset, uint8_t *item, uint32_t itemLen) {
    uint8_t crcBuf[SIZE_CRC] = {0};
    u16 crc16 = 0;
    int32_t ret = 0;
    uint8_t *itemBuf = NULL;
    uint32_t itemBufLen = itemLen + SIZE_CRC;

    crc16 = (u16) libCrc16(item, itemLen);
    //    TraceD("file(name=%s,offset=%d) crc16=%d \r\n", fileName, offset, crc16);
    intToBytes(crc16, crcBuf, sizeof(crcBuf));
    itemBuf = GET_MEM(itemBufLen);
    if (itemBuf == NULL) {
        return ERR_NOK;
    }
    memset(itemBuf, 0, itemBufLen);
    memcpy(itemBuf, crcBuf, sizeof(crcBuf));
    memcpy(itemBuf + sizeof(crcBuf), item, itemLen);
    ret = OOP_CALL(file(), insert, fileName, itemBuf, offset, itemBufLen);
    //    TraceD("ret=%d fileName=%s offset=%d itemBufLen=%d \r\n", ret, fileName, offset, itemBufLen);
    //    TraceHexD("itemBuf=", itemBuf, itemBufLen);
    FREE_MEM(itemBuf);
    return ret == FILE_ERR_OK ? ERR_OK : ERR_NOK;
}

static int32_t readItemFromMemory(uint8_t *memory, uint32_t offset, uint8_t *item, uint32_t itemLen) {
    uint32_t crcLen = SIZE_CRC;
    uint8_t crcBytes[SIZE_CRC] = {0};
    u16 crc16 = 0;
    uint32_t n = 0;

    memcpy(crcBytes, memory + offset, crcLen);
    memcpy(item, memory + offset + crcLen, itemLen);
    crc16 = (u16) libCrc16(item, itemLen);

    hexToU32(&n, crcBytes, sizeof(crcBytes));
    return (n == crc16) ? ERR_OK : ERR_NOK;
}

static int32_t readItem(s8 *fileName, uint32_t offset, uint8_t *item, uint32_t itemLen) {
    uint32_t readLen = 0;
    uint8_t *itemBuf = NULL;
    int32_t ret = 0;

    readLen = itemLen + SIZE_CRC;
    itemBuf = GET_MEM(readLen);
    if (itemBuf == NULL) {
        return 0;
    }
    if(OOP_CALL(file(), read, fileName, itemBuf, offset, &readLen) != FILE_ERR_OK) {
        FREE_MEM(itemBuf);
        return 0;
    }
    ret = readItemFromMemory(itemBuf, 0, item, itemLen);
    FREE_MEM(itemBuf);
    return ret;
}

static int32_t readRecordCache(uint8_t *cache, uint32_t cacheLen) {
    return readItem(FILE_RECORD_CACHE, OFFSET_RECORD_CACHE, cache, cacheLen);
}

static bool hasRecordCache() {
    uint8_t *cache = NULL;
    int32_t ret = 0;

    cache = GET_MEM(SIZE_RECORD_CACHE);
    if (cache == NULL) {
        return false;
    }
    memset(cache, 0x00, SIZE_RECORD_CACHE);
    ret = readRecordCache(cache, SIZE_RECORD_CACHE);
    FREE_MEM(cache);
    return ret == ERR_OK ? true : false;
}

static uint32_t recordFileGetNum() {
    uint32_t num = 0;
    uint8_t totalBytes[SIZE_RECORD_TOTAL] = {0};
    uint32_t readLen = 0;

    readLen = sizeof(totalBytes);
    if (OOP_CALL(file(), read, FILE_RECORD, totalBytes, OFFSET_RECORD_TOTAL, &readLen) != FILE_ERR_OK) {
        return 0;
    }

    hexToU32(&num, totalBytes, readLen);
    if (num == RECORD_MAX_SIZE) {
        return num;
    }
    if (hasRecordCache()) {
        num++;
    }
    return num;
}

static bool isRecordFull() {
    return recordFileGetNum() == RECORD_MAX_SIZE ? true : false;
}

static int32_t updateLastRecordIndex(uint32_t index) {
    uint8_t lastIndexBytes[SIZE_RECORD_LAST_INDEX] = {0};

    u32ToHex(lastIndexBytes, index, SIZE_RECORD_LAST_INDEX);
    FileErr_t err = OOP_CALL(file(), insert, FILE_RECORD_LAST_INDEX,
                lastIndexBytes, OFFSET_RECORD_LAST_INDEX, sizeof(lastIndexBytes));
    return err == FILE_ERR_OK ? ERR_OK : ERR_NOK;
}

static int32_t cleanRecordLastIndexFile() {
    uint8_t buf[2] = {0};
    return OOP_CALL(file(), insert, FILE_RECORD_LAST_INDEX, buf, OFFSET_RECORD_LAST_INDEX, sizeof(buf));
}

static int32_t cleanRecordCacheFile() {
    uint8_t *buf = NULL;
    uint32_t bufMaxLen = SIZE_RECORD_CACHE + SIZE_CRC;
    int32_t ret = 0;

    buf = GET_MEM(bufMaxLen);
    if (buf == NULL)
    {
        return ERR_NOK;
    }
    memset(buf, 0, bufMaxLen);
    ret = OOP_CALL(file(), insert, FILE_RECORD_CACHE, buf, OFFSET_RECORD_CACHE, sizeof(buf));
    FREE_MEM(buf);
    return ret == FILE_ERR_OK ? ERR_OK : ERR_NOK;
}

static int32_t cleanRecordIndexFile() {
    uint8_t *buf = NULL;
    uint32_t bufMaxLen = 10 * 1024;
    uint32_t i = 0, dataSize = 0, block = 0, offset = 0, writeLen = 0;
    int32_t ret = 0;
    buf = GET_MEM(bufMaxLen);
    if (buf == NULL) {
        return ERR_NOK;
    }
    memset(buf, 0, bufMaxLen);
    dataSize = SIZE_RECORD_INDEX * RECORD_MAX_SIZE;
    block = (dataSize / bufMaxLen) + ((dataSize % bufMaxLen) != 0 ? 1 : 0);
    offset = getRecordIndexPosition(0);
    for (i = 0; i < block; i++) {
        offset += writeLen;
        writeLen = (i == block - 1) ? (dataSize % bufMaxLen) : bufMaxLen;
        ret = OOP_CALL(file(), insert, FILE_RECORD_INDEX, buf, offset, writeLen);
    }
    FREE_MEM(buf);
    return ret == FILE_ERR_OK ? ERR_OK : ERR_NOK;
}

static int32_t cleanRecordFile() {
    uint8_t buf[2] = {0};
    FileErr_t err = OOP_CALL(file(), insert, FILE_RECORD, buf, OFFSET_RECORD_TOTAL, sizeof(buf));
    return err == FILE_ERR_OK ? ERR_OK : ERR_NOK;
}

static bool writeRecordCache(uint8_t *recordCacheBuf) {
    return writeItem(FILE_RECORD_CACHE, OFFSET_RECORD_CACHE, recordCacheBuf, SIZE_RECORD_CACHE) == ERR_OK ? true : false;
}


static bool isSelectSegment() {
    int32_t lastIndex = 0;

    if ((lastIndex = getLastRecordIndex()) < 0)
    {
        return false;
    }
    //TraceD("isRecordFull()=%d, lastIndex=%d, RECORD_MAX_SIZE-1=%d \r\n", isRecordFull(), lastIndex, (RECORD_MAX_SIZE - 1));
    if (isRecordFull() && (lastIndex != (RECORD_MAX_SIZE - 1)))
    {
        return true;
    }
    return false;
}

static bool readRecord(uint32_t index, uint8_t *recordBuf) {

    if (readRecordCache(recordBuf, SIZE_RECORD_CACHE) == ERR_OK && getLastRecordIndex() == index)
    {
        return true;
    }

    return readItem(FILE_RECORD, getRecordOffset(index), recordBuf, SIZE_RECORD) == ERR_OK ? true : false;
}

static bool writeRecord(uint32_t index, uint8_t *recordBuf) {
    //    TraceD("offset=%d \r\n", getRecordOffset(index));
    return writeItem(FILE_RECORD, getRecordOffset(index), recordBuf, SIZE_RECORD) == ERR_OK ? true : false;
}

static int32_t readRecordIndex(uint32_t index, uint8_t *recordIndex) {
    return readItem(FILE_RECORD_INDEX, getRecordIndexPosition(index), recordIndex, SIZE_RECORD_INDEX);
}

static int32_t readRecordIndexFromMemory(uint8_t *memory, uint32_t offset, uint8_t *recordIndex) {
    return readItemFromMemory(memory, offset, recordIndex, SIZE_RECORD_INDEX);
}

static bool writeRecordIndex(uint32_t index, uint8_t *recordIndexBuf) {
    return writeItem(FILE_RECORD_INDEX, getRecordIndexPosition(index), recordIndexBuf, SIZE_RECORD_INDEX) == ERR_OK ? true : false;
}

static bool isRecordInMemory(RecordMemory *memory, uint32_t index)
{
    if (memory == NULL) {
        return false;
    }
    if (index >= memory->startIndex && index <= memory->endIndex) {
        return true;
    }
    return false;
}

static bool initRecordIndexMemory(RecordMemory *memory, int32_t *memoryItemNum) {
    uint32_t memoryMaxSize = 10 * 1024;

    if (memory == NULL) {
        return false;
    }
    memory->startIndex = -1;
    memory->endIndex = -1;
    *memoryItemNum = memoryMaxSize / SIZE_RECORD_INDEX_ITEM;
    return true;
}

static bool buildMemory(RecordMemory *memory, uint32_t startIndex, uint32_t endIndex) {
    uint32_t offset = 0, readLen = 0;

    if (memory == NULL) {
        return false;
    }
    offset = getRecordIndexPosition(startIndex);
    readLen = (endIndex - startIndex + 1) * SIZE_RECORD_INDEX_ITEM;
    memory->startIndex = startIndex;
    memory->endIndex = endIndex;
    memory->memory = GET_MEM(readLen);
    if (memory->memory == NULL) {
        return false;
    }
    memset(memory->memory, 0, readLen);
    if (OOP_CALL(file(), read, FILE_RECORD_INDEX, memory->memory, offset, &readLen) != FILE_ERR_OK) {
        return false;
    }
    return true;
}

static void freeMemory(RecordMemory *memory) {
    if (memory != NULL && memory->memory != NULL) {
        FREE_MEM(memory->memory);
        memory->memory = NULL;
    }
}

static bool repairRecordIndex(uint32_t index, uint8_t *recordIndex) {
    uint8_t *recordBuf = NULL;
    uint32_t recordBufLen = SIZE_RECORD;
    bool success = false;

    if (index >= RECORD_MAX_SIZE) {
        return false;
    }
    recordBuf = GET_MEM(recordBufLen);
    if (recordBuf == NULL) {
        goto exit;
    }
    memset(recordBuf, 0, recordBufLen);

    if (!readRecord((uint32_t) index, recordBuf)) {
        goto exit;
    }
    memset(recordIndex, 0, SIZE_RECORD_INDEX);
    if (gGetRecordIndexFromRecordFunc(recordBuf, recordIndex) != ERR_OK) {
        goto exit;
    }

    if (writeRecordIndex((uint32_t) index, recordIndex)) {
        success = true;
    }

    exit:
    if (recordBuf != NULL) FREE_MEM(recordBuf);

    return success;
}

static void verifyRepairRecordIndexFile() {
    int32_t index = 0;
    uint32_t size = 0;
    uint8_t *recordIndexBuf = NULL;
    int32_t offset = 0, endIndex = 0, memoryItemNum = 0;
    RecordMemory recordMemory = {0};

    size = recordFileGetNum();
    if (size == 0)  {
        return;
    }

    recordIndexBuf = GET_MEM(SIZE_RECORD_INDEX);
    if (recordIndexBuf == NULL) {
        goto exit;
    }

    initRecordIndexMemory(&recordMemory, &memoryItemNum);

    for (index = 0; index < size; index++) {
        if (!isRecordInMemory(&recordMemory, (uint32_t) index)) {
            freeMemory(&recordMemory);
            endIndex = (index + memoryItemNum) < size ? index + memoryItemNum : size - 1;
            //            TraceD("index=%d endIndex=%d memoryItemNum=%d \r\n", index, endIndex, memoryItemNum);
            if (!buildMemory(&recordMemory, (uint32_t) index, (uint32_t) endIndex))
            {
                goto exit;
            }

        }
        offset = (index - recordMemory.startIndex) * SIZE_RECORD_INDEX_ITEM;
        //        TraceD("offset=%d \r\n", offset);
        if (readRecordIndexFromMemory(recordMemory.memory, (uint32_t) offset, recordIndexBuf) != ERR_OK) {
            memset(recordIndexBuf, 0, SIZE_RECORD_INDEX);
            repairRecordIndex((uint32_t) index, recordIndexBuf);
        }
    }

    exit:
    if (recordIndexBuf != NULL) FREE_MEM(recordIndexBuf);
    freeMemory(&recordMemory);
}

void libRecordInit(uint32_t recordIndexSize, uint32_t recordSize, 
                uint32_t recordMaxNumber, GetRecordIndexFromRecordFunc getRecordIndexFromRecordFunc){
    if (recordIndexSize <= 0 || recordSize <= 0 || recordMaxNumber <= 0 || getRecordIndexFromRecordFunc == NULL) {
        // TraceE("recordIndexSize,recordSize,recordMaxNumber is Err.\r\n");
        return;
    }

    // platformRecordInit(OFFSET_RECORD_LAST_INDEX - OFFSET_RECORD_TOTAL);

    gRecordIndexSize = recordIndexSize;
    gRecordSize = recordSize;
    gRecordMaxNumber = recordMaxNumber;
    gGetRecordIndexFromRecordFunc = getRecordIndexFromRecordFunc;

    verifyRepairRecordIndexFile();

    libRecordCacheFlush();
}

uint32_t libRecordGetNum() {
    return recordFileGetNum();
}

int32_t libRecordGetLastIndex() {
    if (getLastRecordIndex() == 0 && recordFileGetNum() == 0) {
        return ERR_NOK;
    }
    return getLastRecordIndex();
}

static int32_t addRecordSize() {
    uint32_t total = 0, readLen = 0;
    uint8_t totalBytes[SIZE_RECORD_TOTAL] = {0};

    readLen = sizeof(totalBytes);
    if(OOP_CALL(file(), read, FILE_RECORD, totalBytes, OFFSET_RECORD_TOTAL, &readLen) != FILE_ERR_OK) {
        return ERR_NOK;
    }
    hexToU32(&total, totalBytes, readLen);
    total = (total >= RECORD_MAX_SIZE) ? RECORD_MAX_SIZE : (total + 1);
    memset(totalBytes, 0, sizeof(totalBytes));
    u32ToHex(totalBytes, total, sizeof(totalBytes));
    FileErr_t err = OOP_CALL(file(), insert, FILE_RECORD, totalBytes, OFFSET_RECORD_TOTAL, sizeof(totalBytes));
    return err == FILE_ERR_OK ? ERR_OK : ERR_NOK;
}

void libRecordCacheFlush() {
    uint8_t *recordCacheBuf = NULL;
    int32_t lastIndex = 0;

    recordCacheBuf = GET_MEM(SIZE_RECORD_CACHE);
    if (recordCacheBuf == NULL) {
        goto exit;
    }
    memset(recordCacheBuf, 0, SIZE_RECORD_CACHE);

    if (readRecordCache(recordCacheBuf, SIZE_RECORD_CACHE) != ERR_OK) {
        goto exit;
    }

    lastIndex = getLastRecordIndex();
    //    TraceD("------lastIndex=%d------\r\n", lastIndex);
    if (lastIndex < 0) {
        goto exit;
    }
    writeRecord((uint32_t) lastIndex, recordCacheBuf);
    addRecordSize();
    cleanRecordCacheFile();

    exit:
    if (recordCacheBuf != NULL)FREE_MEM(recordCacheBuf);

}

int32_t libRecordAdd(uint8_t *idx, uint8_t *mRecord) {
    int32_t index = 0;

    if (idx == NULL || mRecord == NULL) {
        return ERR_NOK;
    }

    libRecordCacheFlush();


    if (!writeRecordCache(mRecord)){
        return ERR_NOK;
    }
    index = (u16) getLastRecordIndex();
    if (index < 0) {
        cleanRecordCacheFile();
        return ERR_NOK;
    }

    index = (index == (RECORD_MAX_SIZE - 1) || recordFileGetNum() == 1) ? 0 : index + 1;

    if (!writeRecordIndex((uint32_t) index, idx)) {
        cleanRecordCacheFile();
        return ERR_NOK;
    }

    if (updateLastRecordIndex((uint32_t) index) != ERR_OK) {
        cleanRecordCacheFile();
        return ERR_NOK;
    }
    return ERR_OK;
}

int32_t libRecordDelAll() {
    cleanRecordFile();
    cleanRecordLastIndexFile();
    cleanRecordCacheFile();
    cleanRecordIndexFile();
    return ERR_OK;
}

int32_t libRecordUpdate(uint32_t index, uint8_t *recordIndex, uint8_t *record) {
    bool success = true;

    if (index >= RECORD_MAX_SIZE || recordIndex == NULL) {
        return ERR_NOK;
    }

    if (record != NULL) {
        if (hasRecordCache() && getLastRecordIndex() == index) {
            success = writeRecordCache(record);
        } else {
            success = writeRecord(index, record);
        }
    }

    if (success) {
        success = writeRecordIndex(index, recordIndex);
    }

    return success ? ERR_OK : ERR_NOK;
}

static int32_t expandArrayIfNeeded(u16 **matches, uint32_t count, uint32_t *capacity, const uint32_t expansionSize) {
    u16 *temp = NULL;

    if (count >= *capacity) {
        *capacity += expansionSize;
        temp = (u16 *) GET_MEM(sizeof(u16) * (*capacity));
        if (temp == NULL) {
            return ERR_NOK;
        }

        memcpy(temp, *matches, sizeof(u16) * count);

        FREE_MEM(*matches);
        *matches = temp;
    }

    return ERR_OK;
}

static int32_t libRecordReadIndexListEx(uint32_t matchType, void *matchValue, 
            INDEX_NUM_TYPE indexNumType, u16 **indexList, MatchRecordIndexFunc matchFunc) {
    uint8_t *recordIndexItemBuf = NULL;
    uint32_t recordIndexItemBufLen = SIZE_RECORD_INDEX_ITEM;
    uint32_t size = 0;//记录总数
    int32_t lastIndex = 0, index = 0;
    int32_t loopStart, loopEnd;
    bool hasLoopBack = false;
    uint32_t capacityNum = 256;//默认匹配索引数量
    uint32_t expansionSize = 128;//默认匹配扩展大小
    uint32_t count = 0;
    int32_t offset = 0, memoryItemNum = 0, startIndex = 0;
    RecordMemory recordMemory = {0};

    size = recordFileGetNum();//记录总数

    recordIndexItemBuf = GET_MEM(recordIndexItemBufLen);
    if (recordIndexItemBuf == NULL) {
        goto exit;
    }
    memset(recordIndexItemBuf, 0, recordIndexItemBufLen);

    lastIndex = getLastRecordIndex();
    if (lastIndex < 0) {
        goto exit;
    }

    *indexList = (u16 *) GET_MEM(sizeof(u16) * capacityNum);
    if (*indexList == NULL) {
        goto exit;
    }
    //    TraceD("lastIndex=%d \r\n", lastIndex);
    loopStart = lastIndex;
    loopEnd = 0;

    initRecordIndexMemory(&recordMemory, &memoryItemNum);
    //    TraceD("memoryItemNum=%d \r\n", memoryItemNum);
    
    LOOP_RECORD:
    //    TraceD("loopStart=%d loopEnd=%d \r\n", loopStart, loopEnd);
    for (index = loopStart; index >= loopEnd; index--) {
        if (!isRecordInMemory(&recordMemory, (uint32_t) index)) {
            ddi_watchdog_feed();
            freeMemory(&recordMemory);
            startIndex = (index - memoryItemNum - 1) >= loopEnd ? index - memoryItemNum - 1 : loopEnd;
            //            TraceD("startIndex=%d endIndex=%d memoryItemNum=%d\r\n", startIndex, index, memoryItemNum);
            buildMemory(&recordMemory, (uint32_t) startIndex, (uint32_t) index);
        }
        offset = (index - recordMemory.startIndex) * SIZE_RECORD_INDEX_ITEM;
        //        TraceD("offset=%d \r\n", offset);
        if (readRecordIndexFromMemory(recordMemory.memory, (uint32_t) offset, recordIndexItemBuf) != ERR_OK) {
            continue;
        }
        if (matchFunc(matchType, matchValue, recordIndexItemBuf)) {
            if (expandArrayIfNeeded(indexList, count, &capacityNum, expansionSize) != ERR_OK) {
                count = 0;
                goto exit;
            }
            (*indexList)[count++] = (u16) index;
            if (indexNumType == INDEX_NUM_TYPE_ONE) {
                goto exit;
            }
        }
    }

    //    TraceD("hasLoopBack=%d isSelectSegment=%d \r\n", hasLoopBack, isSelectSegment());
    if (!hasLoopBack && isSelectSegment()) {
        loopStart = (int32_t) size - 1;
        loopEnd = lastIndex + 1;
        hasLoopBack = true;
        goto LOOP_RECORD;
    }

    exit:
    if (recordIndexItemBuf != NULL) FREE_MEM(recordIndexItemBuf);
    freeMemory(&recordMemory);
    return (int32_t) count;
}

int32_t libRecordRead(uint32_t matchType, void *matchValue, uint8_t *idx,
            uint8_t *mRecord, MatchRecordIndexFunc matchFunc) {
    int32_t ret = 0;
    u16 *indexList = NULL;

    ret = libRecordReadIndexListEx(matchType, matchValue, INDEX_NUM_TYPE_ONE, &indexList, matchFunc);
    if (ret > 0) {
        if (idx != NULL) {
            readRecordIndex(indexList[0], idx);
        }
        if (mRecord != NULL) {
            readRecord(indexList[0], mRecord);
        }
        ret = (int32_t) indexList[0];
    } else {
        ret = -1;
    }

    if (indexList != NULL) FREE_MEM(indexList);

    return ret;
}

int32_t libRecordReadIndexList(uint32_t matchType, void *matchValue, u16 **indexList, MatchRecordIndexFunc matchFunc) {
    return libRecordReadIndexListEx(matchType, matchValue, INDEX_NUM_TYPE_MORE, indexList, matchFunc);
}

int32_t readByIndex(uint32_t index, uint8_t *idx, uint8_t *mRecord) {
    int32_t ret = ERR_OK;

    if (index >= RECORD_MAX_SIZE) {
        return ERR_NOK;
    }

    if (idx != NULL) {
        ret = readRecordIndex(index, idx);
    }
    if (ret == ERR_OK && mRecord != NULL) {
        ret = readRecord(index, mRecord) ? ERR_OK : ERR_NOK;
    }
    return ret;
}

int32_t convertIndex(CONVERT_INDEX_TYPE convertIndexType, uint32_t index) {
    int32_t lastIndex = 0, convertIndex = 0;
    int32_t offsetIndex = 0;

    if (!SUPPORT_CYCLE_REPLACE_RECORD_FULL) {
        return index;
    }

    if (libRecordGetNum() < RECORD_MAX_SIZE) {
        return index;
    }

    lastIndex = libRecordGetLastIndex();
    if (lastIndex < 0) {
        return ERR_NOK;
    }
    if (index >= RECORD_MAX_SIZE) {
        return ERR_NOK;
    }
    if (lastIndex == RECORD_MAX_SIZE - 1) {
        return index;
    }
    switch (convertIndexType)
    {
        case CONVERT_INDEX_TYPE_TO_REAL:
            offsetIndex = lastIndex + 1;
            convertIndex = (index + offsetIndex) % RECORD_MAX_SIZE;
            break;

        case CONVERT_INDEX_TYPE_TO_ORDER:
            offsetIndex = lastIndex + 1;
            convertIndex = (index + RECORD_MAX_SIZE - offsetIndex) % RECORD_MAX_SIZE;
            break;
    }
    return convertIndex;
}

static int32_t getRecordIndexFromRecord(uint8_t *record, uint8_t *recordIndex) {
    TxnRecord *transRecord = NULL;

    if (record == NULL || recordIndex == NULL) {
        return ERR_NOK;
    }
    transRecord = (TxnRecord *) record;
    memcpy(recordIndex, &transRecord->idx, sizeof(RecordIdx));
    return ERR_OK;
}

static bool matchVoucher(uint32_t matchType, void *matchValue, uint8_t *recordIndex) {
    RecordIdx *transRecordIndex = NULL;

    if (recordIndex == NULL || matchType != MATCH_VOUCHER) {
        return false;
    }

    transRecordIndex = (RecordIdx *) recordIndex;

    if (transRecordIndex->mStatus != STATUS_OK) {
        return false;
    }
    
    return memcmp(matchValue, transRecordIndex->mVoucher, sizeof(transRecordIndex->mVoucher)) == 0 ? true : false;
}

static bool matchTransStatus(uint32_t matchType, void *matchValue, uint8_t *recordIndex) {
    RecordIdx *transRecordIndex = NULL;
    uint8_t transStatus = 0;

    if (recordIndex == NULL || matchType != MATCH_TRANS_STATUS) {
        return false;
    }
    transRecordIndex = (RecordIdx *) recordIndex;
    transStatus = *(uint8_t *) matchValue;
    return transStatus == transRecordIndex->mStatus ? true : false;
}

static bool isTimeInRange(uint8_t *currentTime, uint8_t *startTime, uint8_t *endTime) {
    uint32_t i = 0;

    for (i = 0; i < 6; i++) {
        if (currentTime[i] < startTime[i] || currentTime[i] > endTime[i]) {
            return false;
        }
    }
    return true;
}

static bool matchTime(uint32_t matchType, void *matchValue, uint8_t *recordIndex) {
    RecordIdx *transRecordIndex = NULL;
    uint32_t timeLen = 6;
    uint8_t timeRange[12] = {0}, startTime[6] = {0}, endTime[6] = {0};

    if (recordIndex == NULL || matchType != MATCH_TIME) {
        return false;
    }
    transRecordIndex = (RecordIdx *) recordIndex;
    if (transRecordIndex->mStatus != STATUS_OK) {
        return false;
    }

    memcpy(timeRange, matchValue, sizeof(timeRange));
    memcpy(startTime, timeRange, timeLen);
    memcpy(endTime, timeRange + timeLen, timeLen);

    return isTimeInRange(transRecordIndex->mTime, startTime, endTime);
}


void recordInit() {
    uint32_t recordSize = sizeof(TxnRecord);
    uint32_t recordIndexSize = sizeof(RecordIdx);

    libRecordInit(recordIndexSize, recordSize, RECORD_MAX_SIZE, getRecordIndexFromRecord);
}

uint32_t recordGetNum() {
    return libRecordGetNum();
}

int32_t recordGetLastIndex() {
    int32_t lastIndex = libRecordGetLastIndex();
    if (lastIndex < 0) return ERR_NOK;
    return (uint32_t) convertIndex(CONVERT_INDEX_TYPE_TO_ORDER, (uint32_t) lastIndex);
}

int32_t recordAdd(TxnRecord *record) {
    return libRecordAdd((uint8_t *) &record->idx, (uint8_t *) record);
}

int32_t recordDelAll() {
    return libRecordDelAll();
}

static int32_t recordRead(RECORD_MATCH_TYPE matchType, void *matchValue,
                uint32_t *indexOrder, RecordIdx *recordIndex, TxnRecord*record) {
    int32_t ret = 0;
    uint32_t index = 0;
    if (matchType == MATCH_TIME) {
        // TraceE("no support!!\r\n");
        return ERR_NOK;
    }
    switch (matchType) {
        case MATCH_INDEX:
            index = *(uint32_t *) matchValue;
            index = (uint32_t) convertIndex(CONVERT_INDEX_TYPE_TO_REAL, (uint32_t) index);
            ret = readByIndex(index, (uint8_t *) recordIndex, (uint8_t *) record);
            if (ret == ERR_OK && indexOrder != NULL) {
                *indexOrder = (uint32_t) convertIndex(CONVERT_INDEX_TYPE_TO_ORDER, (uint32_t) index);
            }
            break;

        case MATCH_VOUCHER:
            if ((ret = libRecordRead(matchType, matchValue,
                        (uint8_t *) recordIndex, (uint8_t *) record, matchVoucher)) >= 0) {
                if (indexOrder != NULL) {
                    *indexOrder = (uint32_t) (uint32_t) convertIndex(CONVERT_INDEX_TYPE_TO_ORDER, (uint32_t) ret);
                }
                ret = ERR_OK;
            } else {
                ret = ERR_NOK;
            }
            break;
        case MATCH_TRANS_STATUS:
            if ((ret = libRecordRead(matchType, matchValue, 
                        (uint8_t *) recordIndex, (uint8_t *) record, matchTransStatus)) >= 0) {
                if (indexOrder != NULL) {
                    *indexOrder = (uint32_t) (uint32_t) convertIndex(CONVERT_INDEX_TYPE_TO_ORDER, (uint32_t) ret);
                }
                ret = ERR_OK;
            } else {
                ret = ERR_NOK;
            }
            break;
    }
    return ret;
}

int32_t ReadByIndex(uint32_t index, RecordIdx *recordIndex, TxnRecord *record) {
    return recordRead(MATCH_INDEX, &index, NULL, recordIndex, record);
}

int32_t ReadByVoucher(uint8_t *voucher, RecordIdx *recordIndex, TxnRecord *record) {
    return recordRead(MATCH_VOUCHER, voucher, NULL, recordIndex, record);
}

int32_t recordReadIndexByTime(uint8_t *startTime, uint8_t *endTime, u16 **indexList) {
    uint32_t timeLen = 6;
    uint8_t timeRange[12] = {0};

    memcpy(timeRange, startTime, timeLen);
    memcpy(timeRange + timeLen, endTime, timeLen);
    return libRecordReadIndexList(MATCH_TIME, timeRange, indexList, matchTime);
}

static bool matchTimeAndId(uint32_t matchType, void *matchValue, uint8_t *recordIndex) {
    
}

int32_t recordReadIndexByTimeAndID(uint8_t *startTime, uint8_t *endTime, uint8_t tradeId, u16 **indexList) {
    uint32_t timeLen = 6;
    uint8_t timeRange[13] = {0};

    memcpy(timeRange, startTime, timeLen);
    memcpy(timeRange + timeLen, endTime, timeLen);
    timeRange[12] = tradeId;
    return libRecordReadIndexList(MATCH_TIME, timeRange, indexList, matchTimeAndId);
}

int32_t ReadLastUnknownTrans(TxnRecord *record) {
    uint32_t index = 0;
    uint32_t status = STATUS_UNKNOWN;
    int32_t ret = 0;

    ret = recordRead(MATCH_TRANS_STATUS, &status, &index, NULL, record);
    return ret == ERR_OK ? index : ret;
}

static int32_t recordUpdate(RECORD_MATCH_TYPE matchType, void *matchValue,
                RecordIdx *recordIndex, TxnRecord *record) {
    int32_t ret = 0;
    uint32_t matchIndex = 0;
    RecordIdx transRecordIndex = {0};
    TxnRecord transRecord = {0};

    if (recordIndex == NULL && record == NULL || matchType == MATCH_TIME) {
        return ERR_NOK;
    }

    ret = recordRead(matchType, matchValue, &matchIndex, &transRecordIndex, &transRecord);

    if (ret == ERR_OK) {
        matchIndex = (uint32_t) convertIndex(CONVERT_INDEX_TYPE_TO_REAL, matchIndex);
        ret = libRecordUpdate((uint32_t) matchIndex, recordIndex == NULL ? (uint8_t *) &record->idx : (uint8_t *) recordIndex, (uint8_t *) record);
    }

    return ret;
}

int32_t recordUpdateByIndex(uint32_t index, TxnRecord *record) {
    return recordUpdate(MATCH_INDEX, &index, &record->idx, record);
}

int32_t recordUpdateIndexByIndex(uint32_t index, RecordIdx *recordIndex) {
    return recordUpdate(MATCH_INDEX, &index, recordIndex, NULL);
}

int32_t recordUpdateByVoucher(uint8_t *voucher, TxnRecord *record) {
    return recordUpdate(MATCH_VOUCHER, voucher, NULL, record);
}

int32_t recordUpdateIndexByVoucher(uint8_t *voucher, RecordIdx *recordIndex) {
    return recordUpdate(MATCH_VOUCHER, voucher, recordIndex, NULL);
}

static bool matchTradeid(uint32_t matchType, void *matchValue, uint8_t *recordIndex) {
    if (recordIndex == NULL) {
        return false;
    }

    RecordIdx *transRecordIndex = (RecordIdx *) recordIndex;
    uint8_t type = *(uint8_t *)matchValue;

    if(transRecordIndex->mStatus == STATUS_OK && transRecordIndex->mType == type) {
        return true;
    } else {
        return false;
    }
}

int32_t recordReadLastTrans(TxnRecord *record, uint8_t tradeid) {
    if(libRecordRead(0, &tradeid, NULL, (uint8_t *) record, matchTradeid) >= 0) {
        return ERR_OK;
    } else {
        return ERR_NOK;
    }
}

void recordDump(TxnRecord *tr) {
    return;
    // LOG_D("mType:%d\r\n", tr->idx.mType);
    // LOG_D("mStatus:%d\r\n", tr->idx.mStatus);
    // LOG_D("mCardNo:%s\r\n", tr->mCardNo);
    // LOG_D("mOrderNo:%s\r\n", tr->mOrderNo);
    // LOG_D_HEX("mVoucher", tr->idx.mVoucher, 3);
    // LOG_D_HEX("mTime", tr->idx.mTime, 6);
    // LOG_D_HEX("mAmount", tr->idx.mAmount, 6);
}

char *recordGetAmount(TxnRecord *record, char *amount) {
    // libFormatAmount(amount, record->idx.mAmount);
    return amount;
}

char *recordGetVoucher(TxnRecord *record, char *voucher) {
    bcdToAsc((uint8_t*)voucher, record->idx.mVoucher, sizeof(record->idx.mVoucher));
    return voucher;
}

char *recordGetTimestamp(TxnRecord *record, char *timestamp) {
    char timeAsc[32] = {0};

    strcpy(timeAsc, "20");
    bcdToAsc((uint8_t*)timeAsc + strlen(timeAsc), record->idx.mTime, sizeof(record->idx.mTime));
    // libFormatTime(timeAsc, timestamp);

    return timestamp;
}

char *recordGetCardNo(TxnRecord *record, char *cardNo) {
    char temp[32] = {0};

    // libTransGetMaskCardNo(record->idx.mType, record->mCardNo, temp, sizeof(temp));
    strcpy(cardNo, temp);

    return cardNo;
}

OOP_CTOR(Record) {
    self->init = recordInit;
    self->add = recordAdd;
    self->readByIdx = readByIndex;
    self->getLastIdx = recordGetLastIndex;
}

Record *record() {
    CALL_ONCE(
        OOP_CALL_CTOR(Record, __record);
    );
    return __record;
}