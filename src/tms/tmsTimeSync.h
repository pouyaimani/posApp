#ifndef TMS_TIME_SYNC_H
#define TMS_TIME_SYNC_H
#include "http/httpClient.h"
/* Call from the same task that calls nth.tick. Uses plain HTTP.
 * This only logs /api/ntp/sync; it does not change the device clock. */
HttpClientResult tmsHttpProbeStart(const char* host, uint16_t port);
HttpClientResult tmsHttpProbeCancel(void);
#endif
