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

static bool createEmptyFile(const char* path) {
    int ret;

    if (path == NULL || *path == '\0')
        return false;

    ret = sdkFileCreate(path, 1, 0);

    if (ret != SDK_FILE_OK) {
        LOG_ERROR("Could not create file '%s': error=%d", path, ret);
        return false;
    }

    return true;
}

static FileHandle* open(File* self, const char* path, const char* mode) {
    bool fileExists;
    bool truncate;
    bool append;

    if (path == NULL || mode == NULL)
        return NULL;

    fileExists = exists(self, path);
    truncate   = mode[0] == 'w';
    append     = mode[0] == 'a';

    /*
     * Opening a missing file for reading must fail.
     * It must not silently create an empty file.
     */
    if (!fileExists && mode[0] == 'r') {
        LOG_WARN("File does not exist: %s", path);
        return NULL;
    }

    // /*
    //  * "wb" means create a new empty file.
    //  */
    // if (truncate && fileExists) {
    //     if (sdkFileDel(path) != SDK_FILE_OK) {
    //         LOG_ERROR("Could not truncate file: %s", path);
    //         return NULL;
    //     }

    //     fileExists = false;
    // }

    if (!fileExists) {
        if (!createEmptyFile(path))
            return NULL;
    }

    FileHandle* handle = MEM_ALLOC(sizeof(FileHandle));
    if (handle == NULL) {
        LOG_ERROR("Could not allocate file handle: %s", path);
        return NULL;
    }

    snprintf(handle->path, sizeof(handle->path), "%s", path);

    /*
     * Only append mode starts at EOF.
     * Read and write modes start at offset zero.
     */
    handle->pos = append ? sdkFileGetSize(handle->path) : 0u;

    return handle;
}

static int close(File* self, FileHandle* handle) {
    if (handle) {
        MEM_FREE(handle);
    }
    return 0;
}

static bool seeked = false;

static int seek(File* self, FileHandle* handle, long offset,
                FileSeekOrigin_t origin) {
    u32 size = sdkFileGetSize(handle->path);

    u32 base;
    switch (origin) {
    case FILE_SEEK_ORG_SET:
        base = 0;
        break;
    case FILE_SEEK_ORG_CUR:
        base = handle->pos;
        break;
    case FILE_SEEK_ORG_END:
        base = size;
        break;
    default:
        return -1; // invalid origin
    }
    long newPos = (long)base + offset;
    if (newPos < 0 || (u32)newPos > size) // bounds check as needed
        return -1;
    handle->pos = (u32)newPos;
    return 0;
}

static size_t read(File* self, void* buffer, size_t size, size_t count,
                   FileHandle* handle) {
    uint32_t requested;
    uint32_t length;
    uint32_t fileSize;
    int32_t  ret;

    if (buffer == NULL || handle == NULL || size == 0u || count == 0u)
        return 0u;

    requested = (uint32_t)(size * count);
    fileSize  = sdkFileGetSize(handle->path);

    if (handle->pos >= fileSize)
        return 0u;

    length = requested;

    /* Avoid requesting data beyond EOF. */
    if (length > fileSize - handle->pos)
        length = fileSize - handle->pos;

    ret = sdkFileRead(handle->path, buffer, handle->pos, &length);

    if (ret != SDK_FILE_OK) {
        LOG_ERROR("Read failed: path=%s pos=%u requested=%u error=%d",
                  handle->path, handle->pos, requested, ret);
        return 0u;
    }

    handle->pos += length;
    return length;
}

static size_t write(File* self, const void* buffer, size_t size, size_t count,
                    FileHandle* handle) {
    size_t bytesToWrite = size * count;
    s32    r            = sdkFileInsert(handle->path, (u8*)buffer, handle->pos,
                                        (u32)bytesToWrite);
    if (r != SDK_FILE_OK)
        return 0; // Error
    handle->pos += (u32)bytesToWrite;
    return bytesToWrite;
}

static int removeFile(File* self, const char* path) {
    int ret = sdkFileDel(path);
    return ret == SDK_FILE_OK ? 0 : -1;
}

static size_t overwrite(File* self, const void* buffer, size_t size,
                        size_t count, FileHandle* handle) {
    size_t   bytesToWrite;
    uint32_t resultingSize;
    int32_t  ret;

    if (buffer == NULL || handle == NULL || size == 0u || count == 0u)
        return 0u;

    bytesToWrite = size * count;

    ret = sdkFileWrite(handle->path, (uint8_t*)buffer, (uint32_t)bytesToWrite);

    // ret = sdkFileInsert(handle->path, (u8*)buffer, 0, (u32)bytesToWrite);

    if (ret != SDK_FILE_OK) {
        LOG_ERROR("Overwrite failed: path=%s bytes=%u error=%d", handle->path,
                  (unsigned)bytesToWrite, ret);
        return 0u;
    }

    resultingSize = sdkFileGetSize(handle->path);

    if (resultingSize < bytesToWrite) {
        LOG_ERROR("Invalid size after overwrite: path=%s size=%u expected=%u",
                  handle->path, resultingSize, (unsigned)bytesToWrite);
        return 0u;
    }

    LOG_TRACE("Overwrite succeed : path=%s bytes=%u error=%d", handle->path,
              (unsigned)bytesToWrite, ret);
    /*
     * sdkFileWrite writes from the beginning, so the resulting logical
     * position is bytesToWrite—not old position + bytesToWrite.
     */
    handle->pos = (uint32_t)bytesToWrite;

    return bytesToWrite;
}

static long tell(File* self, FileHandle* handle) { return handle->pos; }

static int flush(File* self, FileHandle* handle) { return 0; }

static long size(File* self, FileHandle* handle) {
    return sdkFileGetSize(handle->path);
}

static int sync(File* self, FileHandle* handle) { return 0; }

static long getFlashFreeSpace(File* self, char* path) {
    uint32_t fs;
    sdkFileGetFreeSpace(&fs);
    return fs;
}

static FileErr_t insert(File* self, char* path, uint8_t* pheSrc,
                        uint32_t siStart, uint32_t siSrclen) {
    return sdkFileInsert(path, pheSrc, siStart, siSrclen);
}

OOP_CTOR(FileT3Rtos) {
    self->base.vtable.close             = close;
    self->base.vtable.exists            = exists;
    self->base.vtable.flush             = flush;
    self->base.vtable.getFlashFreeSpace = getFlashFreeSpace;
    self->base.vtable.insert            = insert;
    self->base.vtable.open              = open;
    self->base.vtable.read              = read;
    self->base.vtable.remove            = removeFile;
    self->base.vtable.seek              = seek;
    self->base.vtable.size              = size;
    self->base.vtable.sync              = sync;
    self->base.vtable.tell              = tell;
    self->base.vtable.write             = write;
    self->base.vtable.overwrite         = overwrite;
}

#endif