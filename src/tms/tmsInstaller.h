#ifndef TMS_INSTALLER_H
#define TMS_INSTALLER_H

#include "tmsPlatform.h"

#define TMS_APUPDATE_HEADER_LEN 4096u
#define TMS_APUPDATE_PATH "/mtd2/apupdate"
#define TMS_SWITCH_FLAG_PATH "/mtd0/switch.flg"

uint32_t tmsCrc32Update(uint32_t crc, const uint8_t *data, size_t length);
bool tmsInstallerVerify(const TmsPlatformOps *platform, const char *path,
                        uint32_t dataOffset, uint32_t dataLength,
                        const char *expectedHash);
bool tmsInstallerPrepare(const TmsPlatformOps *platform, const char *path,
                         const char *payloadName, uint32_t payloadLength,
                         uint32_t payloadCrc);
bool tmsInstallerActivate(const TmsPlatformOps *platform);

#endif
