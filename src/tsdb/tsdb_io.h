#ifndef TSDB_IO_H_
#define TSDB_IO_H_

#include <stdint.h>
#include <stddef.h>

typedef struct tsdb_file_io_handle_t {
    int fd;                      /* Your SDK's file descriptor */
    void* file_ptr;          /* Or a pointer your SDK returns */
    /* Add any other fields your specific SDK requires */
} tsdb_file_io_handle_t; 

/* Standardized seek origins matching POSIX-style semantics */
typedef enum {
    FILE_IO_SEEK_SET = 0,  /* From beginning of file */
    FILE_IO_SEEK_CUR = 1,  /* From current position */
    FILE_IO_SEEK_END = 2   /* From end of file */
} tsdb_file_io_seek_origin_t;

/* Forward declaration - opaque handle, implementation defined by user */
typedef struct tsdb_file_io_handle_t tsdb_file_io_handle_t;

/* The main interface struct */
typedef struct {
    
    /* Open a file and return an opaque handle.
     * Parameters:
     *   path: file path/identifier
     *   mode: "rb", "wb", "ab", "rb+", "wb+", "ab+" etc.
     * Returns: handle pointer on success, NULL on failure */
    tsdb_file_io_handle_t* (*open)(const char* path, const char* mode);
    
    /* Close a previously opened file handle.
     * Returns: 0 on success, non-zero on failure */
    int (*close)(tsdb_file_io_handle_t* handle);
    
    /* Read data from file.
     * Parameters:
     *   buffer: destination buffer
     *   size: size of each element
     *   count: number of elements
     *   handle: file handle from open()
     * Returns: number of elements successfully read */
    size_t (*read)(void* buffer, size_t size, size_t count, tsdb_file_io_handle_t* handle);
    
    /* Write data to file.
     * Parameters:
     *   buffer: source buffer
     *   size: size of each element
     *   count: number of elements
     *   handle: file handle from open()
     * Returns: number of elements successfully written */
    size_t (*write)(const void* buffer, size_t size, size_t count, tsdb_file_io_handle_t* handle);
    
    /* Seek to position in file.
     * Parameters:
     *   handle: file handle from open()
     *   offset: byte offset
     *   origin: reference point (SEEK_SET, SEEK_CUR, or SEEK_END)
     * Returns: 0 on success, non-zero on failure */
    int (*seek)(tsdb_file_io_handle_t* handle, long offset, tsdb_file_io_seek_origin_t origin);
    
    /* Get current position in file.
     * Returns: current byte position, -1 on error */
    long (*tell)(tsdb_file_io_handle_t* handle);
    
    /* Flush/sync any buffered writes to storage.
     * Returns: 0 on success, non-zero on failure */
    int (*flush)(tsdb_file_io_handle_t* handle);
    
    /* Get file size.
     * Returns: file size in bytes, -1 on error */
    long (*size)(tsdb_file_io_handle_t* handle);
    
    /* Check if file exists (optional, for convenience).
     * Returns: 1 if exists, 0 if not */
    int (*exists)(const char* path);
    
    /* Delete a file (optional).
     * Returns: 0 on success, non-zero on failure */
    int (*remove)(const char* path);

    int (*sync)(tsdb_file_io_handle_t* handle);
    
} tsdb_file_io_t;

#endif