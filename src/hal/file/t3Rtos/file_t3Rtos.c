#ifdef DEVICE_TRENDITT3RTOS

#include "file_t3Rtos.h"
#include "posplatform.h"
#include <sdkKey.h>
#include "sdkemvapp.h"
#include "sdkFile.h"
#include "sys/sys.h"
#include "logger.h"

enum {
    T3_FILE_READ   = (1u << 0),
    T3_FILE_WRITE  = (1u << 1),
    T3_FILE_APPEND = (1u << 2)
};

/**
 * @brief Parse a standard C file-opening mode.
 *
 * Supported:
 *   r, rb, r+, rb+, r+b
 *   w, wb, w+, wb+, w+b
 *   a, ab, a+, ab+, a+b
 *   wx and valid combinations with b and +
 *
 * @return true if the mode is valid.
 */
static bool parseOpenMode(const char* mode, uint8_t* flags, bool* truncateFile,
                          bool* exclusiveCreate) {
    bool    plus      = false;
    bool    binary    = false;
    bool    exclusive = false;
    uint8_t result    = 0u;

    if (mode == NULL || mode[0] == '\0' || flags == NULL ||
        truncateFile == NULL || exclusiveCreate == NULL) {
        return false;
    }

    *truncateFile    = false;
    *exclusiveCreate = false;

    switch (mode[0]) {
    case 'r':
        result = T3_FILE_READ;
        break;

    case 'w':
        result        = T3_FILE_WRITE;
        *truncateFile = true;
        break;

    case 'a':
        result = T3_FILE_WRITE | T3_FILE_APPEND;
        break;

    default:
        return false;
    }

    for (const char* p = mode + 1; *p != '\0'; ++p) {
        switch (*p) {
        case '+':
            if (plus)
                return false;
            plus = true;
            break;

        case 'b':
            if (binary)
                return false;
            binary = true;
            break;

        case 'x':
            if (exclusive)
                return false;
            exclusive = true;
            break;

        default:
            return false;
        }
    }

    /* Exclusive creation is valid only for w modes. */
    if (exclusive && mode[0] != 'w')
        return false;

    if (plus)
        result |= T3_FILE_READ | T3_FILE_WRITE;

    *flags           = result;
    *exclusiveCreate = exclusive;

    return true;
}

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

/**
 * @brief Open a file using standard fopen-style mode semantics.
 *
 * The underlying SDK uses path-based operations instead of
 * native file descriptors. Therefore, FileHandle maintains
 * the logical position and requested access permissions.
 *
 * @param self File interface.
 * @param path File path.
 * @param mode Standard C fopen mode.
 *
 * @return Allocated FileHandle on success, NULL on failure.
 */
static FileHandle* open(File* self, const char* path, const char* mode) {
    FileHandle* handle;
    uint8_t     flags;
    bool        truncateFile;
    bool        exclusiveCreate;
    bool        fileExists;
    int         ret;

    if (path == NULL || path[0] == '\0' || mode == NULL) {
        LOG_ERROR("FILE OPEN: Invalid arguments");
        return NULL;
    }

    if (!parseOpenMode(mode, &flags, &truncateFile, &exclusiveCreate)) {
        LOG_ERROR("FILE OPEN: Invalid mode '%s'", mode);
        return NULL;
    }

    /*
     * Allocate and validate the handle before any potentially
     * destructive filesystem operation.
     */
    handle = MEM_ALLOC(sizeof(*handle));

    if (handle == NULL) {
        LOG_ERROR("FILE OPEN: Allocation failed: %s", path);
        return NULL;
    }

    memset(handle, 0, sizeof(*handle));

    ret = snprintf(handle->path, sizeof(handle->path), "%s", path);

    if (ret < 0 || (size_t)ret >= sizeof(handle->path)) {
        LOG_ERROR("FILE OPEN: Path too long: %s", path);
        goto fail;
    }

    fileExists = exists(self, handle->path);

    LOG_TRACE("FILE OPEN: path=%s mode=%s exists=%d", handle->path, mode,
              (int)fileExists);

    /*
     * r / r+:
     * Existing file required.
     */
    if (mode[0] == 'r' && !fileExists) {
        LOG_WARN("FILE OPEN: File not found: %s", handle->path);
        goto fail;
    }

    /*
     * wx / w+x:
     * Fail if the file already exists.
     */
    if (exclusiveCreate && fileExists) {
        LOG_WARN("FILE OPEN: Exclusive create failed: %s", handle->path);
        goto fail;
    }

    /*
     * w / w+:
     * Existing file must be truncated to zero bytes.
     *
     * This SDK does not expose a verified truncate operation,
     * so deletion and recreation are used.
     */
    if (truncateFile && fileExists) {
        ret = sdkFileDel(handle->path);

        if (ret != SDK_FILE_OK) {
            LOG_ERROR("FILE OPEN: Truncate delete failed: "
                      "path=%s error=%d",
                      handle->path, ret);
            goto fail;
        }

        fileExists = false;

        LOG_TRACE("FILE OPEN: Existing file deleted: %s", handle->path);
    }

    /*
     * w / w+ / a / a+:
     * Create a missing file.
     */
    if (!fileExists) {
        if (!createEmptyFile(handle->path)) {
            LOG_ERROR("FILE OPEN: Create failed: %s", handle->path);
            goto fail;
        }

        LOG_TRACE("FILE OPEN: File created: %s", handle->path);
    }

    handle->modeFlags = flags;

    /*
     * Append modes start at EOF.
     * All other modes start at offset zero.
     */
    if ((flags & T3_FILE_APPEND) != 0u) {
        handle->pos = sdkFileGetSize(handle->path);
    } else {
        handle->pos = 0u;
    }

    LOG_TRACE("FILE OPEN: Success path=%s mode=%s "
              "flags=0x%02X pos=%u",
              handle->path, mode, (unsigned)handle->modeFlags,
              (unsigned)handle->pos);

    return handle;

fail:
    MEM_FREE(handle);
    return NULL;
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

    if ((handle->modeFlags & T3_FILE_READ) == 0u) {
        LOG_ERROR("FILE READ: File not opened for reading: %s", handle->path);
        return 0u;
    }

    if (count > SIZE_MAX / size || count > UINT32_MAX / size) {
        LOG_ERROR("FILE READ: Invalid requested length");
        return 0u;
    }

    requested = (uint32_t)(size * count);

    fileSize = sdkFileGetSize(handle->path);

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
    size_t   bytesToWrite;
    uint32_t fileSize;
    int32_t  ret;

    if (buffer == NULL || handle == NULL || size == 0u || count == 0u) {
        return 0u;
    }

    if ((handle->modeFlags & T3_FILE_WRITE) == 0u) {
        LOG_ERROR("FILE WRITE: Write not permitted: %s", handle->path);
        return 0u;
    }

    if (count > SIZE_MAX / size || count > UINT32_MAX / size) {
        LOG_ERROR("FILE WRITE: Invalid length");
        return 0u;
    }

    bytesToWrite = size * count;
    fileSize     = sdkFileGetSize(handle->path);

    /*
     * Append mode must position each write at the current EOF,
     * not merely at the EOF recorded when the file was opened.
     */
    if ((handle->modeFlags & T3_FILE_APPEND) != 0u) {
        handle->pos = fileSize;
    } else if (handle->pos != fileSize) {
        /*
         * sdkFileInsert may shift existing data rather than
         * overwrite it. Reject this operation until a verified
         * positional-overwrite implementation is available.
         */
        LOG_ERROR("FILE WRITE: Positional overwrite unsupported: "
                  "path=%s pos=%u fileSize=%u",
                  handle->path, (unsigned)handle->pos, (unsigned)fileSize);
        return 0u;
    }

    if (bytesToWrite > UINT32_MAX - handle->pos) {
        LOG_ERROR("FILE WRITE: Position overflow: %s", handle->path);
        return 0u;
    }

    ret = sdkFileInsert(handle->path, (uint8_t*)buffer, handle->pos,
                        (uint32_t)bytesToWrite);

    if (ret != SDK_FILE_OK) {
        LOG_ERROR("FILE WRITE: Failed path=%s pos=%u "
                  "bytes=%u error=%d",
                  handle->path, (unsigned)handle->pos, (unsigned)bytesToWrite,
                  ret);
        return 0u;
    }

    handle->pos += (uint32_t)bytesToWrite;

    LOG_TRACE("FILE WRITE: Success path=%s bytes=%u pos=%u", handle->path,
              (unsigned)bytesToWrite, (unsigned)handle->pos);

    return bytesToWrite;
}

static int removeFile(File* self, const char* path) {
    LOG_TRACE("Deleteing file : %s", path);
    if (!exists(self, path)) {
        LOG_TRACE("file (%s) is not existed", path);
        return 0;
    }
    int ret = sdkFileDel(path);
    LOG_TRACE("deleting file (%s) result = %d", path, ret);
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