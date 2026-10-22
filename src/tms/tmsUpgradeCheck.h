#ifndef TMS_UPGRADE_CHECK_H
#define TMS_UPGRADE_CHECK_H
#include "tmsApi.h"
TmsApiResult tmsUpgradeCheckStart(const char *host, uint16_t port,
                                 const char *serial, const char *brand,
                                 const char *current_version);
TmsApiResult tmsUpgradeCheckCancel(void);
#endif
