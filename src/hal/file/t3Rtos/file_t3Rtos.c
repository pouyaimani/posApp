#ifdef DEVICE_TRENDITT3RTOS

#include "file_t3Rtos.h"
#include "posplatform.h"
#include <sdkKey.h>

static bool isExist(File *self, char *path) {
    return sdkFileIsExist(path);
}

static FileErr_t create(File *self, char *filename, uint32_t len, uint8_t initValue) {
    sdkFileCreate(filename, len, initValue);
}

static FileErr_t read(File *self, char *path, uint8_t *pheDest,
                uint32_t siStart, uint32_t *psiDestLen) {
    return sdkFileRead(path, pheDest, siStart, psiDestLen);
}

static FileErr_t write(File *self, char *path, u8 *pheSrc, u32 siSrcLen) {
    return sdkFileWrite(path, pheSrc, siSrcLen);
}

static FileErr_t append(File *self, char *path, uint8_t *pheSrc,
                uint32_t siSrcLen) {
    return sdkFileAppend(path, pheSrc, siSrcLen);
}

static FileErr_t insert(File *self, char *path, uint8_t *pheSrc,
                uint32_t siStart, uint32_t siSrclen) {
    return sdkFileInsert(path, pheSrc, siStart, siSrclen);
}

static FileErr_t delete(File *self, char *path) {
    return sdkFileDel(path);
}

static uint32_t getSize(File *self, char *path) {
    sdkFileGetSize(path);
}

static uint32_t getFreeSpace(File *self, char *freeSpace) {
    return sdkFileGetFreeSpace(freeSpace);
}

OOP_CTOR(FileT3Rtos) {
}

#endif