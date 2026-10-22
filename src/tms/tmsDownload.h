#ifndef TMS_DOWNLOAD_H
#define TMS_DOWNLOAD_H
#include "http/httpClient.h"
#ifndef TMS_DOWNLOAD_FILE_HEADER
#define TMS_DOWNLOAD_FILE_HEADER "file/file.h"
#endif
#include TMS_DOWNLOAD_FILE_HEADER

typedef enum {
    TMS_DL_OK                = 0,
    TMS_DL_INVALID           = -1,
    TMS_DL_BUSY              = -2,
    TMS_DL_URL               = -3,
    TMS_DL_HTTPS_UNSUPPORTED = -4,
    TMS_DL_HTTP              = -5,
    TMS_DL_NETWORK           = -6,
    TMS_DL_FILE              = -7,
    TMS_DL_SIZE              = -8,
    TMS_DL_CANCELLED         = -9
} TmsDownloadResult;
typedef struct TmsDownload TmsDownload;
typedef struct {
    TmsDownloadResult result;
    HttpClientStatus  http;
    uint32_t          written, expected;
    bool              cleanup_failed;
} TmsDownloadStatus;
typedef void (*TmsDownloadDone)(TmsDownload*, const TmsDownloadStatus*, void*);
typedef void (*TmsDownloadProgress)(uint32_t written, uint32_t total, void*);
/* Zero-initialize once. Use only on NTH's owner task. Do not mutate in flight.
 */
struct TmsDownload {
    HttpClient client;
    char headers[2048], host[NTH_MAX_HOST_LEN], authority[NTH_MAX_HOST_LEN + 8];
    char target[1024], path[128];
    uint16_t            port;
    FileHandle*         handle;
    TmsDownloadStatus   status;
    TmsDownloadDone     done;
    TmsDownloadProgress progress;
    void*               user;
    bool                busy, in_callback, owns_file;
};
/* Plain HTTP full GET. No redirects/resume/install/hash verification.
 * URL includes its original encoded query. temp_path must be a dedicated
 * staging file: an existing file there is deleted after response headers are
 * accepted. Strings are copied; object lives until completion. Nonzero start:
 * no callback. Accepted start: exactly one done callback. Queue further work
 * outside callbacks. timeout_ms is the existing NTH phase timeout, not an
 * inactivity timeout. */
TmsDownloadResult tms_download_start(TmsDownload* d, const char* url,
                                     const char*         temp_path,
                                     uint32_t            expected_size,
                                     uint32_t            timeout_ms,
                                     TmsDownloadProgress progress,
                                     TmsDownloadDone done, void* user);
TmsDownloadResult tms_download_cancel(TmsDownload* d);
#endif
