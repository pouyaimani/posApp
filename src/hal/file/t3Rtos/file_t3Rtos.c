#ifdef DEVICE_TRENDITT3RTOS

#include "file_t3Rtos.h"
#include "posplatform.h"
#include <sdkKey.h>
#include "sdkemvapp.h"
#include "sdkFile.h"

static FileErr_t translateSskErr(int err) {
    FileErr_t ret = FILE_ERR_NONE;
    switch (err) {
    case SDK_FILE_CRCERR:
        ret = FILE_ERR_CRCERR;
        break;
    case SDK_FILE_ERROR:
        ret = FILE_ERROR;
        break;
    case SDK_FILE_SEEK_ERROR:
        ret = FILE_SEEK_ERROR;
        break;
    case SDK_FILE_EOF:
        ret = FILE_ERR_EOF;
        break;
    case SDK_FILE_OK:
        ret = FILE_ERR_OK;
        break;
    default:
        break;
    }
    return ret;
}

static bool isExist(File *self, char *path) {
    return sdkFileIsExist(path);
}

static FileErr_t create(File *self, char *filename, uint32_t len, uint8_t initValue) {
    return translateSskErr(sdkFileCreate(filename, len, initValue));
}

static FileErr_t read(File *self, char *path, uint8_t *pheDest,
                uint32_t siStart, uint32_t *psiDestLen) {
    return translateSskErr(sdkFileRead(path, pheDest, siStart, psiDestLen));
}

static FileErr_t write(File *self, char *path, u8 *pheSrc, u32 siSrcLen) {
    return translateSskErr(sdkFileWrite(path, pheSrc, siSrcLen));
}

static FileErr_t append(File *self, char *path, uint8_t *pheSrc,
                uint32_t siSrcLen) {
    return translateSskErr(sdkFileAppend(path, pheSrc, siSrcLen));
}

static FileErr_t insert(File *self, char *path, uint8_t *pheSrc,
                uint32_t siStart, uint32_t siSrclen) {
    return translateSskErr(sdkFileInsert(path, pheSrc, siStart, siSrclen));
}

static FileErr_t delete(File *self, char *path) {
    return translateSskErr(sdkFileDel(path));
}

static uint32_t getSize(File *self, char *path) {
    return sdkFileGetSize(path);
}

static uint32_t getFreeSpace(File *self, char *freeSpace) {
    return sdkFileGetFreeSpace(freeSpace);
}

OOP_CTOR(FileT3Rtos) {
    self->base.vtable.append = append;
    self->base.vtable.create = create;
    self->base.vtable.delete = delete;
    self->base.vtable.getFreeSpace = getFreeSpace;
    self->base.vtable.getSize = getSize;
    self->base.vtable.insert = insert;
    self->base.vtable.isExist = isExist;
    self->base.vtable.read = read;
    self->base.vtable.write = write;
}

#endif