#include "esp_tsdb.h"
#include "file/file.h"
#include "dev/dev.h"

const char *param_names[] = {"SOC", "Vbat", "PpvTotal", "Pload", "Pcharge"};

static int tsdbf_close(tsdb_file_io_handle_t *handle) {
    if (!handle) {
        return;
    }
    int ret = OOP_CALL(file(), close, (FileHandle *)handle->file_ptr);
    FREE_MEM(handle);
    return ret;
}

static int tsdbf_exists(const char *path) {
    return OOP_CALL(file(), exists, path);
}

static int tsdbf_flush(tsdb_file_io_handle_t *handle) {
    return OOP_CALL(file(), flush, (FileHandle *)handle->file_ptr);
}

static tsdb_file_io_handle_t *tsdbf_open(const char *path, const char *mode) {
    FileHandle *fh = OOP_CALL(file(), open, path, mode);
    if (!fh) {
        return NULL;
    }
    tsdb_file_io_handle_t *handle = GET_MEM(sizeof(tsdb_file_io_handle_t));
    handle->file_ptr = (tsdb_file_io_handle_t*)fh;
    return handle;
}

static int tsdbf_read(void *buffer, size_t size, size_t count, tsdb_file_io_handle_t *handle) {
    return OOP_CALL(file(), read, buffer, size, count, (FileHandle *)handle->file_ptr);
}

static int tsdbf_write(void *buffer, size_t size, size_t count, tsdb_file_io_handle_t *handle) {
    return OOP_CALL(file(), write, buffer, size, count, (FileHandle *)handle->file_ptr);
}

static int tsdbf_remove(const char *path) {
    return OOP_CALL(file(), remove, path);
}

static int tsdbf_seek(tsdb_file_io_handle_t *handle, long offset, tsdb_file_io_seek_origin_t origin) {
    return OOP_CALL(file(), seek, (FileHandle *)handle->file_ptr, offset, (FileSeekOrigin_t)origin);
}

static long tsdbf_size(tsdb_file_io_handle_t *handle) {
    return OOP_CALL(file(), size, (FileHandle *)handle->file_ptr);
}

static int tsdbf_sync(tsdb_file_io_handle_t *handle) {
    return OOP_CALL(file(), sync, (FileHandle *)handle->file_ptr);
}

static long tsdbf_tell(tsdb_file_io_handle_t *handle) {
    return OOP_CALL(file(), tell, (FileHandle *)handle->file_ptr);
}

tsdb_file_io_t tsdb_file_io = {
    .close = tsdbf_close,
    .exists = tsdbf_exists,
    .flush = tsdbf_flush,
    .open = tsdbf_open,
    .read = tsdbf_read,
    .remove = tsdbf_remove,
    .seek = tsdbf_seek,
    .size = tsdbf_size,
    .sync = tsdbf_sync,
    .tell = tsdbf_tell,
    .write = tsdbf_write
};