#include "tmsDownloadProbe.h"
#include "logger.h"
#include <string.h>

static TmsDownload* download;
static char*        queued_url;
static uint32_t     queued_size;
static bool         pending;
static unsigned     last_percent;

#define sizeof_queued_url 1024

#define TMS_UPG_FILE "/mtd0/tms-download.part"

TmsDownloadResult tmsDownloadProbeInit() {
    download = MEM_ALLOC(sizeof(TmsDownload));
    RETURN_VALUE_IF_NULL(download, ;, TMS_DL_CANCELLED);
    queued_url = MEM_ALLOC(sizeof_queued_url);
    RETURN_VALUE_IF_NULL(queued_url, ;, TMS_DL_CANCELLED);
    return TMS_DL_OK;
}

static void progress(uint32_t written, uint32_t total, void* user) {
    unsigned percent = (unsigned)((uint64_t)written * 100 / total);
    (void)user;
    if (percent >= last_percent + 10 || percent == 100) {
        LOG_TRACE("TMS download: %u%% (%lu/%lu)", percent,
                  (unsigned long)written, (unsigned long)total);
        last_percent = percent;
    }
}

static void done(TmsDownload* d, const TmsDownloadStatus* s, void* user) {
    (void)d;
    (void)user;
    LOG_TRACE("TMS download done: result=%d HTTP=%u client=%d parser=%d "
              "bytes=%lu/%lu cleanup_failed=%d",
              (int)s->result, s->http.http_status, (int)s->http.result,
              (int)s->http.http_result, (unsigned long)s->written,
              (unsigned long)s->expected, (int)s->cleanup_failed);
    if (s->result == TMS_DL_OK)
        LOG_TRACE("TMS package saved: /mtd2/tms-download.part (size checked; "
                  "not installed)");
}

TmsDownloadResult tmsDownloadProbeQueue(const char* url, uint32_t expected) {
    if (pending || download->busy || download->in_callback)
        return TMS_DL_BUSY;
    if (!url || !*url || strlen(url) >= sizeof_queued_url || !expected)
        return TMS_DL_INVALID;
    strcpy(queued_url, url);
    queued_size = expected;
    pending     = true;
    return TMS_DL_OK;
}

void tmsDownloadProbeTick(void) {
    TmsDownloadResult rc;
    /* Temporary diagnostic: logs once per 1000 calls. */
    static unsigned diagnosticTicks = 0;

    // if (download->busy && ++diagnosticTicks >= 1000) {
    //     NthTransaction* tx = download->client.tx;
    //     diagnosticTicks    = 0;

    //     LOG_TRACE("DL waiting: written=%lu/%lu clientBusy=%d",
    //               (unsigned long)download->status.written,
    //               (unsigned long)download->status.expected,
    //               (int)download->client.busy);

    //     if (tx != NULL) {
    //         LOG_TRACE("DL NTH: active=%d state=%d fd=%d "
    //                   "peerClosed=%d timeout=%lu",
    //                   (int)tx->active, (int)tx->state, tx->socketFd,
    //                   (int)tx->peerClosed, (unsigned long)tx->timeoutMs);
    //     }
    // }
    if (!pending || download->busy || download->in_callback)
        return;
    pending      = false;
    last_percent = 0;
    rc = tms_download_start(download, queued_url, TMS_UPG_FILE, queued_size,
                            300000, progress, done, NULL);
    if (rc != TMS_DL_OK)
        LOG_TRACE("TMS download start rejected: result=%d", (int)rc);
}

TmsDownloadResult tmsDownloadProbeCancel(void) {
    pending = false;
    return tms_download_cancel(download);
}
