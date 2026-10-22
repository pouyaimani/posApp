#ifndef TMS_DOWNLOAD_PROBE_H
#define TMS_DOWNLOAD_PROBE_H

#include "tmsDownload.h"

TmsDownloadResult tmsDownloadProbeInit();

/* Safe to queue from the update-check callback. Copies URL and size. */
TmsDownloadResult tmsDownloadProbeQueue(const char* url, uint32_t expected);

/* Call after nth.tick returns, from the same owner task. */
void tmsDownloadProbeTick(TmsDownloadProgress cb);

TmsDownloadResult tmsDownloadProbeCancel(void);

#endif
