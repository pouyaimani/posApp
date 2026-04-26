#ifdef DEVICE_TRENDITT3RTOS

#include "file_t3Rtos.h"
#include "posplatform.h"
#include <sdkKey.h>
#include "sdkemvapp.h"
#include "sdkFile.h"
#include "dev/dev.h"

static bool exists(File* self, const char* path) {
    return sdkFileIsExist(path);
}

static FileHandle* open(File *self, const char* path, const char* mode) {
    if (!exists(self, path)) {
        if (sdkFileCreate(path, 0, 0) != SDK_FILE_OK) {
            return NULL;
        }
    }
    FileHandle *handle = GET_MEM(sizeof(FileHandle));
    snprintf(handle->path, sizeof(handle->path), "%s", path);
    return handle;
}

static int close(File *self, FileHandle *handle) {
    if (handle) {
        FREE_MEM(handle);
    }
    return 0;
}

static size_t read(File* self, void* buffer, size_t size, size_t count, FileHandle* handle) {
    uint32_t len = size * count;
    sdkFileRead(handle->path, buffer, handle->pos, &len);
}

static size_t write(File* self, const void* buffer, size_t size, size_t count, FileHandle* handle) {
    return sdkFileWrite(handle->path, buffer, size * count);
}

static int seek(File* self, FileHandle* handle, long offset, FileSeekOrigin_t origin) {
    u32 size = sdkFileGetSize(handle->path);

    switch (origin) {
        case FILE_SEEK_SET: // SEEK_SET
            handle->pos = offset;
            break;
        case FILE_SEEK_CUR: // SEEK_CUR
            handle->pos += offset;
            break;
        case FILE_SEEK_END: // SEEK_END
            handle->pos = size + offset;
            break;
        default:
            return -1;
    }

    return 0;
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
    return sdkFileDel(path);
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