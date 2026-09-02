#ifndef TMS_DOWNLOADER_H
#define TMS_DOWNLOADER_H

#include "tmsHttpClient.h"
#include "tmsPlatform.h"

#define TMS_DOWNLOAD_URL_MAX 512u
#define TMS_DOWNLOAD_PATH_MAX 128u
#define TMS_DOWNLOAD_RANGE_SIZE 4096u

typedef enum {
    TMS_DOWNLOAD_IDLE = 0,
    TMS_DOWNLOAD_RUNNING,
    TMS_DOWNLOAD_COMPLETED,
    TMS_DOWNLOAD_FAILED,
    TMS_DOWNLOAD_CANCELLED
} TmsDownloadState;

typedef struct TmsDownloader TmsDownloader;
typedef void (*TmsDownloadDoneCallback)(TmsDownloader *downloader,
                                        bool success, void *userData);

struct TmsDownloader {
    TmsHttpClient http;
    const TmsPlatformOps *platform;
    char host[TMS_HTTP_HOST_MAX_LEN];
    uint16_t port;
    char path[TMS_DOWNLOAD_URL_MAX];
    char filePath[TMS_DOWNLOAD_PATH_MAX];
    uint32_t bodyOffset;
    uint32_t totalSize;
    uint32_t downloaded;
    uint32_t requestedStart;
    uint32_t requestedLength;
    uint32_t headerReserve;
    TmsDownloadState state;
    bool pendingRequest;
    TmsDownloadDoneCallback onDone;
    void *userData;
};

void tmsDownloaderInit(TmsDownloader *downloader,
                       const TmsPlatformOps *platform);
HttpFlowResult tmsDownloaderStart(TmsDownloader *downloader, const char *url,
                                  const char *filePath, uint32_t totalSize,
                                  uint32_t headerReserve,
                                  TmsDownloadDoneCallback callback,
                                  void *userData);
void tmsDownloaderCancel(TmsDownloader *downloader);
void tmsDownloaderProcess(TmsDownloader *downloader);
uint32_t tmsDownloaderProgress(const TmsDownloader *downloader);

#endif
