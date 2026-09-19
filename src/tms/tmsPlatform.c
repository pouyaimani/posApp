#include "tmsPlatform.h"
#include "file/file.h"
#include "sys/sys.h"

static int removeFile(const char* path) { OOP_CALL(file(), remove, path); }

static int createFile(const char* path, uint32_t size) {
    sdkFileCreate(path, size, 0);
}

static int readFile(const char* path, uint32_t offset, uint8_t* data,
                    size_t requested, size_t* actual) {}

static int writeFile(const char* path, uint32_t offset, const uint8_t* data,
                     size_t length) {}
static uint32_t fileSize(const char* path) {}

static int syncFiles(void) {}

static int setUpdateFlag(void) {}

static void reboot(void) {}

static const TmsPlatformOps __tmsPlatformOps = {
    .removeFile    = removeFile,
    .createFile    = createFile,
    .readFile      = readFile,
    .writeFile     = writeFile,
    .fileSize      = fileSize,
    .syncFiles     = syncFiles,
    .setUpdateFlag = setUpdateFlag,
    .reboot        = reboot,
};

const TmsPlatformOps* tmsPlatformOps() { return &__tmsPlatformOps; }