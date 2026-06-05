#ifdef DEVICE_TRENDITT3RTOS

#include "file_t3Rtos.h"
#include "posplatform.h"
#include <sdkKey.h>
#include "sdkemvapp.h"
#include "sdkFile.h"
#include "sys/sys.h"
#include "logger.h"

static bool exists(File* self, const char* path) {
    return sdkFileIsExist(path);
}

static FileHandle* open(File *self, const char* path, const char* mode) {
    if (!exists(self, path)) {
        LOG_WARN("file (%s) is not exists.", path);
        int ret = sdkFileCreate(path, 2, 0);
        if (ret != SDK_FILE_OK) {
            LOG_ERROR("file (%s) could not be created. error = %d", path, ret);
            u32 fs;
            sdkFileGetFreeSpace(&fs);
            LOG_ERROR("flash free space = %u", fs);
            return NULL;
        }
    }
    FileHandle *handle = MEM_ALLOC(sizeof(FileHandle));
    snprintf(handle->path, sizeof(handle->path), "%s", path);
    handle->pos = sdkFileGetSize(handle->path);
    return handle;
}

static int close(File *self, FileHandle *handle) {
    if (handle) {
        MEM_FREE(handle);
    }
    return 0;
}

static bool seeked = false;

static int seek(File* self, FileHandle* handle, long offset, FileSeekOrigin_t origin) {
    u32 size = sdkFileGetSize(handle->path);

    u32 base;
    switch(origin) {
        case FILE_SEEK_ORG_SET: base = 0; break;
        case FILE_SEEK_ORG_CUR: base = handle->pos; break;
        case FILE_SEEK_ORG_END: base = size; break;
        default:       return -1; // invalid origin
    }
    long newPos = (long)base + offset;
    if (newPos < 0 || (u32)newPos > size) // bounds check as needed
        return -1;
    handle->pos = (u32)newPos;
    return 0;
}


static size_t read(File* self, void* buffer, size_t size, size_t count, FileHandle* handle) {
    u32 len = (u32)size * count;
    s32 r = sdkFileRead(handle->path, buffer, handle->pos, &len);
    if (r != SDK_FILE_OK)
        return 0; // Error
    handle->pos += len; // advance the position
    return len;
}

static size_t write(File* self, const void* buffer, size_t size, size_t count, FileHandle* handle) {
    size_t bytesToWrite = size * count;
    s32 r = sdkFileInsert(handle->path, (u8*)buffer, handle->pos, (u32)bytesToWrite);
    if (r != SDK_FILE_OK)
        return 0; // Error
    handle->pos += (u32)bytesToWrite;
    return bytesToWrite;
}

static long tell(File* self, FileHandle* handle) {
    return handle->pos;
}

static int flush(File* self, FileHandle* handle) {
    return 0;
}

static long size(File* self, FileHandle* handle) {
    return sdkFileGetSize(handle->path);
}

static int removeFile(File* self, const char* path) {
    int ret = sdkFileDel(path);
    return ret == SDK_FILE_OK ? 0 : -1;
}

static int sync(File* self, FileHandle* handle) {
    return 0;
}

static long getFreeSpace(File* self, char *path) {
    return sdkFileGetFreeSpace(path);
}

static FileErr_t insert(File *self, char *path, uint8_t *pheSrc,
                uint32_t siStart, uint32_t siSrclen) {
    return sdkFileInsert(path, pheSrc, siStart, siSrclen);
}

OOP_CTOR(FileT3Rtos) {
    self->base.vtable.close = close;
    self->base.vtable.exists = exists;
    self->base.vtable.flush = flush;
    self->base.vtable.getFreeSpace = getFreeSpace;
    self->base.vtable.insert = insert;
    self->base.vtable.open = open;
    self->base.vtable.read = read;
    self->base.vtable.remove = removeFile;
    self->base.vtable.seek = seek;
    self->base.vtable.size = size;
    self->base.vtable.sync = sync;
    self->base.vtable.tell = tell;
    self->base.vtable.write = write;
}

#endif