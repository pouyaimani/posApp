#ifndef FILE_H_
#define FILE_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FILE_ERR_NONE,
    FILE_ERR_CRCERR,
    FILE_ERROR,
    FILE_SEEK_ERROR,
    FILE_ERR_EOF,
    FILE_ERR_OK
} FileErr_t;

typedef enum {
    FILE_OPEN_R,
    FILE_OPEN_W,
    FILE_OPEN_A,

    FILE_OPEN_RR,
    FILE_OPEN_WW,
    FILE_OPEN_AA,
} FileOpenMode_t;

OOP_CLASS(FileHandle) {
    int   fd;
    void* file_ptr;
    char  path[128];
    long  pos;
};

typedef enum {
    FILE_SEEK_ORG_SET = 0, /* From beginning of file */
    FILE_SEEK_ORG_CUR = 1, /* From current position */
    FILE_SEEK_ORG_END = 2  /* From end of file */
} FileSeekOrigin_t;

OOP_DECLARE_CLASS(File)

OOP_VTABLE(File) {
    /* Open a file and return an opaque handle.
     * Parameters:
     *   path: file path/identifier
     *   mode: "rb", "wb", "ab", "rb+", "wb+", "ab+" etc.
     * Returns: handle pointer */
    OOP_IMETHOD(FileHandle*, File, open, const char* path, const char* mode);

    /* Close a previously opened file handle.
     * Returns: 0 on success, non-zero on failure */
    OOP_IMETHOD(int, File, close, FileHandle*);

    /* Read data from file.
     * Parameters:
     *   buffer: destination buffer
     *   size: size of each element
     *   count: number of elements
     *   handle: file handle from open()
     * Returns: number of elements successfully read */
    OOP_IMETHOD(size_t, File, read, void* buffer, size_t size, size_t count,
                FileHandle* handle);

    /* Write data to file.
     * Parameters:
     *   buffer: source buffer
     *   size: size of each element
     *   count: number of elements
     *   handle: file handle from open()
     * Returns: number of elements successfully written */
    OOP_IMETHOD(size_t, File, write, const void* buffer, size_t size,
                size_t count, FileHandle* handle);

    /* Seek to position in file.
     * Parameters:
     *   handle: file handle from open()
     *   offset: byte offset
     *   origin: reference point (SEEK_SET, SEEK_CUR, or SEEK_END)
     * Returns: 0 on success, non-zero on failure */
    OOP_IMETHOD(int, File, seek, FileHandle* handle, long offset,
                FileSeekOrigin_t origin);

    /* Get current position in file.
     * Returns: current byte position, -1 on error */
    OOP_IMETHOD(long, File, tell, FileHandle* handle);

    /* Flush/sync any buffered writes to storage.
     * Returns: 0 on success, non-zero on failure */
    OOP_IMETHOD(int, File, flush, FileHandle* handle);

    /* Get file size.
     * Returns: file size in bytes, -1 on error */
    OOP_IMETHOD(long, File, size, FileHandle* handle);

    /* Check if file exists (optional, for convenience).
     * Returns: 1 if exists, 0 if not */
    OOP_IMETHOD(bool, File, exists, const char* path);

    /* Delete a file (optional).
     * Returns: 0 on success, non-zero on failure */
    OOP_IMETHOD(int, File, remove, const char* path);

    OOP_IMETHOD(int, File, sync, FileHandle* handle);

    OOP_IMETHOD(long, File, getFreeSpace, char*);

    OOP_IMETHOD(FileErr_t, File, insert, char*, uint8_t*, uint32_t, uint32_t);
};

OOP_CLASS(File) { OOP_IMPLEMENTS(File); };

OOP_CTOR(File);

File* file(void);

#endif