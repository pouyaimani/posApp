#include "tmsInstaller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t tmsCrc32Update(uint32_t crc, const uint8_t *data, size_t length)
{
    size_t i;
    unsigned bit;
    if (data == NULL)
        return crc;
    for (i = 0u; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1u) ^ (0xEDB88320u &
                                 (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc;
}

bool tmsInstallerVerify(const TmsPlatformOps *platform, const char *path,
                        uint32_t dataOffset, uint32_t dataLength,
                        const char *expectedHash)
{
    uint8_t buffer[4096];
    uint32_t position = 0u;
    uint32_t crc = 0xFFFFFFFFu;
    char actual[9];
    if (platform == NULL || platform->readFile == NULL || path == NULL ||
        expectedHash == NULL)
        return false;
    while (position < dataLength) {
        size_t requested = dataLength - position;
        size_t actualLength = 0u;
        if (requested > sizeof(buffer))
            requested = sizeof(buffer);
        if (platform->readFile(path, dataOffset + position, buffer, requested,
                               &actualLength) != 0 ||
            actualLength != requested)
            return false;
        crc = tmsCrc32Update(crc, buffer, actualLength);
        position += (uint32_t)actualLength;
    }
    crc ^= 0xFFFFFFFFu;
    snprintf(actual, sizeof(actual), "%08lX", (unsigned long)crc);
    return strtoul(expectedHash, NULL, 16) == strtoul(actual, NULL, 16);
}

bool tmsInstallerPrepare(const TmsPlatformOps *platform, const char *path,
                         const char *payloadName, uint32_t payloadLength,
                         uint32_t payloadCrc)
{
    uint8_t header[TMS_APUPDATE_HEADER_LEN];
    int length;
    if (platform == NULL || platform->writeFile == NULL || path == NULL ||
        payloadName == NULL)
        return false;
    memset(header, 0, sizeof(header));
    length = snprintf((char *)header, sizeof(header),
                      "[namelist]\r\nfile0=%s\r\n"
                      "[adrlist]\r\nadr0=0x%08lX\r\n"
                      "[lenlist]\r\nlen0=%lu\r\n"
                      "[crclist]\r\ncrc0=0x%08lX\r\n",
                      payloadName, (unsigned long)TMS_APUPDATE_HEADER_LEN,
                      (unsigned long)payloadLength,
                      (unsigned long)payloadCrc);
    if (length < 0 || (size_t)length >= sizeof(header))
        return false;
    return platform->writeFile(path, 0u, header, sizeof(header)) == 0;
}

bool tmsInstallerActivate(const TmsPlatformOps *platform)
{
    static const uint8_t flag = '1';
    if (platform == NULL)
        return false;
    if (platform->setUpdateFlag != NULL) {
        if (platform->setUpdateFlag() != 0)
            return false;
    } else {
        if (platform->createFile == NULL || platform->writeFile == NULL)
            return false;
        if (platform->removeFile != NULL)
            platform->removeFile(TMS_SWITCH_FLAG_PATH);
        if (platform->createFile(TMS_SWITCH_FLAG_PATH, 1u) != 0 ||
            platform->writeFile(TMS_SWITCH_FLAG_PATH, 0u, &flag, 1u) != 0)
            return false;
    }
    return platform->syncFiles == NULL || platform->syncFiles() == 0;
}
