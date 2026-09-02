#include "tmsDownloader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parseUrl(const char *url, char *host, size_t hostCapacity,
                     uint16_t *port, char *path, size_t pathCapacity)
{
    const char *authority;
    const char *slash;
    const char *colon;
    size_t authorityLength;
    size_t hostLength;
    unsigned long parsedPort = 80u;
    if (url == NULL || host == NULL || port == NULL || path == NULL)
        return false;
    if (strncmp(url, "http://", 7u) == 0)
        authority = url + 7u;
    else if (strncmp(url, "https://", 8u) == 0)
        return false; /* Current Nth flow is TCP-only; do not silently downgrade. */
    else
        authority = url;
    slash = strchr(authority, '/');
    authorityLength = slash != NULL ? (size_t)(slash - authority)
                                    : strlen(authority);
    colon = memchr(authority, ':', authorityLength);
    hostLength = colon != NULL ? (size_t)(colon - authority) : authorityLength;
    if (hostLength == 0u || hostLength >= hostCapacity)
        return false;
    memcpy(host, authority, hostLength);
    host[hostLength] = '\0';
    if (colon != NULL) {
        char portText[8];
        size_t portLength = authorityLength - hostLength - 1u;
        if (portLength == 0u || portLength >= sizeof(portText))
            return false;
        memcpy(portText, colon + 1, portLength);
        portText[portLength] = '\0';
        parsedPort = strtoul(portText, NULL, 10);
        if (parsedPort == 0u || parsedPort > 65535u)
            return false;
    }
    if (slash == NULL)
        slash = "/";
    if (strlen(slash) >= pathCapacity)
        return false;
    strcpy(path, slash);
    *port = (uint16_t)parsedPort;
    return true;
}

static void complete(TmsDownloader *downloader, bool success)
{
    TmsDownloadDoneCallback callback = downloader->onDone;
    void *userData = downloader->userData;
    downloader->state = success ? TMS_DOWNLOAD_COMPLETED : TMS_DOWNLOAD_FAILED;
    if (callback != NULL)
        callback(downloader, success, userData);
}

static HttpFlowResult requestNext(TmsDownloader *downloader);

static void rangeDone(TmsHttpClient *http, HttpFlowResult result,
                      const HttpResponse *response, const uint8_t *body,
                      size_t bodyLength, void *userData)
{
    TmsDownloader *downloader = (TmsDownloader *)userData;
    uint32_t expected;
    (void)http;
    if (downloader == NULL || downloader->state != TMS_DOWNLOAD_RUNNING)
        return;
    expected = downloader->requestedLength;
    if (result != HTTP_FLOW_OK || response == NULL || body == NULL ||
        (response->statusCode != 206 &&
         !(response->statusCode == 200 && downloader->requestedStart == 0u &&
           expected == downloader->totalSize)) ||
        bodyLength != expected ||
        downloader->platform->writeFile(
            downloader->filePath,
            downloader->headerReserve + downloader->requestedStart, body,
            bodyLength) != 0) {
        complete(downloader, false);
        return;
    }
    downloader->downloaded += (uint32_t)bodyLength;
    if (downloader->downloaded == downloader->totalSize) {
        complete(downloader, true);
        return;
    }
    downloader->pendingRequest = true;
}

static HttpFlowResult requestNext(TmsDownloader *downloader)
{
    HttpFlowHeader headers[2];
    char rangeValue[64];
    uint32_t remaining = downloader->totalSize - downloader->downloaded;
    uint32_t length = remaining < TMS_DOWNLOAD_RANGE_SIZE
                          ? remaining
                          : TMS_DOWNLOAD_RANGE_SIZE;
    int written;
    downloader->requestedStart = downloader->downloaded;
    downloader->requestedLength = length;
    written = snprintf(rangeValue, sizeof(rangeValue), "bytes=%lu-%lu",
                       (unsigned long)downloader->requestedStart,
                       (unsigned long)(downloader->requestedStart + length - 1u));
    if (written < 0 || (size_t)written >= sizeof(rangeValue))
        return HTTP_FLOW_ERR_REQUEST;
    headers[0].name = "Range";
    headers[0].value = rangeValue;
    headers[1].name = "Connection";
    headers[1].value = "close";
    return tmsHttpClientRequest(&downloader->http, HTTP_METHOD_GET,
                                downloader->path, headers, 2u, NULL, 0u,
                                rangeDone, downloader);
}

void tmsDownloaderInit(TmsDownloader *downloader,
                       const TmsPlatformOps *platform)
{
    if (downloader == NULL)
        return;
    memset(downloader, 0, sizeof(*downloader));
    downloader->platform = platform;
}

HttpFlowResult tmsDownloaderStart(TmsDownloader *downloader, const char *url,
                                  const char *filePath, uint32_t totalSize,
                                  uint32_t headerReserve,
                                  TmsDownloadDoneCallback callback,
                                  void *userData)
{
    if (downloader == NULL || downloader->platform == NULL ||
        downloader->platform->createFile == NULL ||
        downloader->platform->writeFile == NULL || url == NULL ||
        filePath == NULL || totalSize == 0u || callback == NULL)
        return HTTP_FLOW_ERR_INVALID_ARG;
    if (!parseUrl(url, downloader->host, sizeof(downloader->host),
                  &downloader->port, downloader->path,
                  sizeof(downloader->path)))
        return HTTP_FLOW_ERR_REQUEST;
    if (strlen(filePath) >= sizeof(downloader->filePath))
        return HTTP_FLOW_ERR_REQUEST;
    strcpy(downloader->filePath, filePath);
    downloader->totalSize = totalSize;
    downloader->downloaded = 0u;
    downloader->headerReserve = headerReserve;
    downloader->onDone = callback;
    downloader->userData = userData;
    if (downloader->platform->removeFile != NULL)
        downloader->platform->removeFile(filePath);
    if (downloader->platform->createFile(filePath,
                                         headerReserve + totalSize) != 0)
        return HTTP_FLOW_ERR_NO_RESOURCE;
    tmsHttpClientInit(&downloader->http, downloader->host, downloader->port);
    downloader->state = TMS_DOWNLOAD_RUNNING;
    downloader->pendingRequest = false;
    return requestNext(downloader);
}

void tmsDownloaderCancel(TmsDownloader *downloader)
{
    if (downloader == NULL || downloader->state != TMS_DOWNLOAD_RUNNING)
        return;
    tmsHttpClientCancel(&downloader->http);
    downloader->state = TMS_DOWNLOAD_CANCELLED;
}

void tmsDownloaderProcess(TmsDownloader *downloader)
{
    if (downloader == NULL || downloader->state != TMS_DOWNLOAD_RUNNING ||
        !downloader->pendingRequest)
        return;
    downloader->pendingRequest = false;
    if (requestNext(downloader) != HTTP_FLOW_OK)
        complete(downloader, false);
}

uint32_t tmsDownloaderProgress(const TmsDownloader *downloader)
{
    if (downloader == NULL || downloader->totalSize == 0u)
        return 0u;
    return (uint32_t)(((uint64_t)downloader->downloaded * 100u) /
                      downloader->totalSize);
}
