#include "fileInterface.h"
#include "embedDB_mem.h"
#include "sys/sys.h"
#include "file/file.h"
#include "logger.h"
#include "debug_print.h"

typedef struct {
    char*       filename;
    FileHandle* file;
} FILE_INFO;

void* setupFile(const char* filename) {
    FILE_INFO* fileInfo = EMDB_MEM_ALLOC(sizeof(FILE_INFO));
    int        nameLen  = strlen(filename);
    fileInfo->filename  = EMDB_MEM_ALLOC(nameLen + 1);
    memset(fileInfo->filename, 0, nameLen + 1);
    memcpy(fileInfo->filename, filename, nameLen);
    fileInfo->file = NULL;
    return fileInfo;
}

void tearDownFile(void* finfo) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    EMDB_MEM_FREE(fileInfo->filename);
    if (fileInfo->file != NULL)
        OOP_CALL(file(), close, fileInfo->file);
    EMDB_MEM_FREE(finfo);
}

int8_t FILE_REMOVE(void* finfo) {
    if (finfo == NULL)
        return 1;
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;

    if (fileInfo->file != NULL) {
        OOP_CALL(file(), close, fileInfo->file);
        fileInfo->file = NULL;
    }

    int8_t result = 1;
    if (fileInfo->filename != NULL) {
        if (OOP_CALL(file(), remove, fileInfo->filename) != 0) {
            result = 0;
#ifdef PRINT_ERRORS
            LOG_DEBUG("ERROR: Failed to remove temp file");
#endif
        }
        return result;
    }
}

int8_t FILE_READ(void* buffer, uint32_t pageNum, uint32_t pageSize,
                 void* finfo) {
    // LOG_DEBUG("FILE_READ: page num = %d, page size = %d.", pageNum,
    // pageSize);
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    OOP_CALL(file(), seek, (FileHandle*)fileInfo->file, pageSize * pageNum,
             FILE_SEEK_ORG_SET);
    size_t readLen = OOP_CALL(file(), read, buffer, pageSize, 1,
                              (FileHandle*)fileInfo->file);
    return readLen == pageSize ? 1 : 0;
}

int8_t FILE_WRITE(void* buffer, uint32_t pageNum, uint32_t pageSize,
                  void* finfo) {
    // LOG_DEBUG("FILE_WRITE: page num = %d, page size = %d.", pageNum,
    // pageSize);
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    OOP_CALL(file(), seek, (FileHandle*)fileInfo->file, pageSize * pageNum,
             FILE_SEEK_ORG_SET);
    size_t written = OOP_CALL(file(), write, buffer, pageSize, 1,
                              (FileHandle*)fileInfo->file);
    return written == pageSize ? 1 : 0;
}

int8_t FILE_ERASE(uint32_t startPage, uint32_t endPage, uint32_t pageSize,
                  void* file) {
    return 1;
}

int8_t FILE_CLOSE(void* finfo) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    OOP_CALL(file(), close, (FileHandle*)fileInfo->file);
    fileInfo->file = NULL;
    return 1;
}

int8_t FILE_FLUSH(void* finfo) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    return OOP_CALL(file(), flush, (FileHandle*)fileInfo->file) == 0;
}

int8_t FILE_OPEN(void* finfo, uint8_t mode) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    if (mode == EMBEDDB_FILE_MODE_W_PLUS_B) {
        fileInfo->file = OOP_CALL(file(), open, fileInfo->filename, "w+b");
    } else if (mode == EMBEDDB_FILE_MODE_R_PLUS_B) {
        fileInfo->file = OOP_CALL(file(), open, fileInfo->filename, "r+b");
    } else {
        return 0;
    }
    LOG_TRACE("file [%s] is opened", fileInfo->file->path);
    if (fileInfo->file == NULL) {
        LOG_DEBUG("fileInfo->file is NULL", fileInfo->file->path);
        return 0;
    } else {
        return 1;
    }
}

int8_t FILE_ERR(void* file) {
    FILE_INFO* fileInfo = (FILE_INFO*)file;
    LOG_ERROR("FILE_ERR[%s]", fileInfo->file);
    return -1;
}

int8_t FILE_EOF(void* file) {
    FILE_INFO* fileInfo = (FILE_INFO*)file;
    LOG_ERROR("FILE_EOF[%s]", fileInfo->file);
    return -1;
}

int8_t FILE_READ_REL(void* buffer, uint32_t size, uint32_t n, void* finfo) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    return OOP_CALL(file(), read, buffer, size, n, (FileHandle*)fileInfo->file);
}

int8_t FILE_WRITE_REL(void* buffer, uint32_t size, uint32_t n, void* finfo) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    return OOP_CALL(file(), write, buffer, size, n,
                    (FileHandle*)fileInfo->file);
}

int8_t FILE_SEEK(uint32_t n, void* finfo) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    return OOP_CALL(file(), seek, (FileHandle*)fileInfo->file, n,
                    FILE_SEEK_ORG_SET);
}

int32_t FILE_TELL(void* finfo) {
    FILE_INFO* fileInfo = (FILE_INFO*)finfo;
    return OOP_CALL(file(), tell, (FileHandle*)fileInfo->file);
}

char* tempFilePath(void) {
    static char tempPathBuffer[256];
    debug_log(tempPathBuffer, sizeof(tempPathBuffer), "embeddb_%lu.tmp",
              (unsigned long)rand());

    char* out = EMDB_MEM_ALLOC(strlen(tempPathBuffer) + 1);
    if (out) {
        strcpy(out, tempPathBuffer);
    }
    return out;
}

embedDBFileInterface* getFileInterface() {
    embedDBFileInterface* fileInterface =
        EMDB_MEM_ALLOC(sizeof(embedDBFileInterface));
    fileInterface->close        = FILE_CLOSE;
    fileInterface->read         = FILE_READ;
    fileInterface->write        = FILE_WRITE;
    fileInterface->erase        = FILE_ERASE;
    fileInterface->open         = FILE_OPEN;
    fileInterface->flush        = FILE_FLUSH;
    fileInterface->error        = FILE_ERR;
    fileInterface->eof          = FILE_EOF;
    fileInterface->readRel      = FILE_READ_REL;
    fileInterface->writeRel     = FILE_WRITE_REL;
    fileInterface->seek         = FILE_SEEK;
    fileInterface->tell         = FILE_TELL;
    fileInterface->setup        = setupFile;
    fileInterface->teardown     = tearDownFile;
    fileInterface->removeFile   = FILE_REMOVE;
    fileInterface->tempFilePath = tempFilePath;
    return fileInterface;
}