#include "tmsDownloader.h"
#include "logger.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------
 * URL parsing
 * ----------------------------------------------------------- */

/**
 * @brief Parse an HTTP URL into host, port, and request path.
 *
 * HTTPS is intentionally rejected because the current transport is
 * TCP-only. Do not log the complete URL: its query string may contain
 * credentials or signed download tokens.
 */
static bool parseUrl(const char* url, char* host, size_t hostCapacity,
                     uint16_t* port, char* path, size_t pathCapacity) {
    const char*   authority;
    const char*   slash;
    const char*   colon;
    size_t        authorityLength;
    size_t        hostLength;
    unsigned long parsedPort = 80u;

    LOG_TRACE("TMS DL parseUrl(): begin hostCap=%lu pathCap=%lu",
              (unsigned long)hostCapacity, (unsigned long)pathCapacity);

    if (url == NULL || host == NULL || port == NULL || path == NULL ||
        hostCapacity == 0u || pathCapacity == 0u) {
        LOG_ERROR("TMS DL parseUrl(): invalid arguments url=%d host=%d "
                  "port=%d path=%d hostCap=%lu pathCap=%lu",
                  (int)(url != NULL), (int)(host != NULL), (int)(port != NULL),
                  (int)(path != NULL), (unsigned long)hostCapacity,
                  (unsigned long)pathCapacity);
        return false;
    }

    if (strncmp(url, "http://", 7u) == 0) {
        authority = url + 7u;
        LOG_TRACE("TMS DL parseUrl(): HTTP scheme detected");
    } else if (strncmp(url, "https://", 8u) == 0) {
        LOG_ERROR(
            "TMS DL parseUrl(): HTTPS is unsupported by TCP-only transport");
        return false;
    } else {
        authority = url;
        LOG_TRACE(
            "TMS DL parseUrl(): no scheme, treating input as HTTP authority");
    }

    slash = strchr(authority, '/');
    authorityLength =
        (slash != NULL) ? (size_t)(slash - authority) : strlen(authority);
    colon = memchr(authority, ':', authorityLength);
    hostLength =
        (colon != NULL) ? (size_t)(colon - authority) : authorityLength;

    LOG_TRACE("TMS DL parseUrl(): authorityLen=%lu hostLen=%lu explicitPort=%d",
              (unsigned long)authorityLength, (unsigned long)hostLength,
              (int)(colon != NULL));

    if (hostLength == 0u || hostLength >= hostCapacity) {
        LOG_ERROR(
            "TMS DL parseUrl(): invalid/oversized host hostLen=%lu cap=%lu",
            (unsigned long)hostLength, (unsigned long)hostCapacity);
        return false;
    }

    memcpy(host, authority, hostLength);
    host[hostLength] = '\0';

    if (colon != NULL) {
        char   portText[8];
        char*  portEnd    = NULL;
        size_t portLength = authorityLength - hostLength - 1u;

        if (portLength == 0u || portLength >= sizeof(portText)) {
            LOG_ERROR("TMS DL parseUrl(): invalid port text length=%lu",
                      (unsigned long)portLength);
            return false;
        }

        memcpy(portText, colon + 1, portLength);
        portText[portLength] = '\0';
        parsedPort           = strtoul(portText, &portEnd, 10);

        if (portEnd == portText || *portEnd != '\0' || parsedPort == 0u ||
            parsedPort > 65535u) {
            LOG_ERROR("TMS DL parseUrl(): invalid numeric port");
            return false;
        }
    }

    if (slash == NULL)
        slash = "/";

    {
        size_t pathLength = strlen(slash);
        if (pathLength >= pathCapacity) {
            LOG_ERROR("TMS DL parseUrl(): path too long len=%lu cap=%lu",
                      (unsigned long)pathLength, (unsigned long)pathCapacity);
            return false;
        }

        memcpy(path, slash, pathLength + 1u);
        LOG_TRACE("TMS DL parseUrl(): success port=%lu pathLen=%lu", parsedPort,
                  (unsigned long)pathLength);
    }

    *port = (uint16_t)parsedPort;
    return true;
}

/* -----------------------------------------------------------
 * Completion
 * ----------------------------------------------------------- */

/**
 * @brief Finish the download and notify its owner exactly once per call.
 *
 * The callback may release/reuse the downloader. Do not access the
 * downloader after invoking it.
 */
static void complete(TmsDownloader* downloader, bool success) {
    TmsDownloadDoneCallback callback;
    void*                   userData;

    if (downloader == NULL) {
        LOG_ERROR("TMS DL complete(): downloader is NULL");
        return;
    }

    callback = downloader->onDone;
    userData = downloader->userData;
    LOG_TRACE("TMS DL complete(): completing from state=%d success=%d",
              (int)downloader->state, (int)success);

    downloader->state = success ? TMS_DOWNLOAD_COMPLETED : TMS_DOWNLOAD_FAILED;
    downloader->pendingRequest = false;

    LOG_TRACE("TMS DL complete(): newState=%d downloaded=%lu/%lu "
              "progress=%lu%% callbackSet=%d",
              (int)downloader->state, (unsigned long)downloader->downloaded,
              (unsigned long)downloader->totalSize,
              (unsigned long)(downloader->totalSize != 0u
                                  ? ((uint64_t)downloader->downloaded * 100u) /
                                        downloader->totalSize
                                  : 0u),
              (int)(callback != NULL));

    /* Preserve existing behavior: do not release HTTP here. */
    if (callback != NULL) {
        LOG_TRACE("TMS DL complete(): invoking completion callback");
        callback(downloader, success, userData);
        /* No downloader access after the callback. */
    } else {
        LOG_ERROR("TMS DL complete(): completion callback is NULL");
    }
}

static HttpFlowResult requestNext(TmsDownloader* downloader);

/* -----------------------------------------------------------
 * Range response callback
 * ----------------------------------------------------------- */

/**
 * @brief Validate and persist a completed HTTP range response.
 *
 * This retains the original HTTP acceptance policy: 206 for ranges,
 * or 200 only when the request covers the entire file from offset 0.
 */
static void rangeDone(TmsHttpClient* http, HttpFlowResult result,
                      const HttpResponse* response, const uint8_t* body,
                      size_t bodyLength, void* userData) {
    TmsDownloader* downloader = (TmsDownloader*)userData;
    uint32_t       expected;
    int      httpStatus = (response != NULL) ? (int)response->statusCode : -1;
    int      writeResult;
    uint32_t writeOffset;

    (void)http;

    LOG_TRACE("TMS DL rangeDone(): HTTP result=%d status=%d bodyLen=%lu",
              (int)result, httpStatus, (unsigned long)bodyLength);

    if (downloader == NULL) {
        LOG_ERROR("TMS DL rangeDone(): downloader context is NULL");
        return;
    }

    if (downloader->state != TMS_DOWNLOAD_RUNNING) {
        LOG_TRACE("TMS DL rangeDone(): ignored callback state=%d (not RUNNING)",
                  (int)downloader->state);
        return;
    }

    expected = downloader->requestedLength;
    LOG_TRACE("TMS DL rangeDone(): start=%lu expected=%lu "
              "downloaded=%lu total=%lu",
              (unsigned long)downloader->requestedStart,
              (unsigned long)expected, (unsigned long)downloader->downloaded,
              (unsigned long)downloader->totalSize);

    if (result != HTTP_FLOW_OK) {
        LOG_ERROR("TMS DL rangeDone(): HTTP flow failed result=%d status=%d",
                  (int)result, httpStatus);
        complete(downloader, false);
        return;
    }

    if (response == NULL) {
        LOG_ERROR("TMS DL rangeDone(): HTTP response is NULL");
        complete(downloader, false);
        return;
    }

    if (body == NULL) {
        LOG_ERROR("TMS DL rangeDone(): response body is NULL len=%lu",
                  (unsigned long)bodyLength);
        complete(downloader, false);
        return;
    }

    if (httpStatus != 206 &&
        !(httpStatus == 200 && downloader->requestedStart == 0u &&
          expected == downloader->totalSize)) {
        LOG_ERROR("TMS DL rangeDone(): unexpected HTTP status=%d "
                  "start=%lu expected=%lu total=%lu",
                  httpStatus, (unsigned long)downloader->requestedStart,
                  (unsigned long)expected,
                  (unsigned long)downloader->totalSize);
        complete(downloader, false);
        return;
    }

    if (bodyLength != (size_t)expected) {
        LOG_ERROR(
            "TMS DL rangeDone(): body length mismatch actual=%lu expected=%lu",
            (unsigned long)bodyLength, (unsigned long)expected);
        complete(downloader, false);
        return;
    }

    if (downloader->downloaded > downloader->totalSize ||
        bodyLength > (size_t)(downloader->totalSize - downloader->downloaded)) {
        LOG_ERROR("TMS DL rangeDone(): received bytes exceed declared size "
                  "downloaded=%lu received=%lu total=%lu",
                  (unsigned long)downloader->downloaded,
                  (unsigned long)bodyLength,
                  (unsigned long)downloader->totalSize);
        complete(downloader, false);
        return;
    }

    if (downloader->platform == NULL ||
        downloader->platform->writeFile == NULL) {
        LOG_ERROR("TMS DL rangeDone(): platform writeFile unavailable");
        complete(downloader, false);
        return;
    }

    writeOffset = downloader->headerReserve + downloader->requestedStart;

    LOG_TRACE("TMS DL rangeDone(): writing file offset=%lu size=%lu",
              (unsigned long)writeOffset, (unsigned long)bodyLength);

    writeResult = downloader->platform->writeFile(
        downloader->filePath, writeOffset, body, bodyLength);

    if (writeResult != 0) {
        LOG_ERROR("TMS DL rangeDone(): file write failed result=%d "
                  "offset=%lu size=%lu",
                  writeResult, (unsigned long)writeOffset,
                  (unsigned long)bodyLength);
        complete(downloader, false);
        return;
    }

    downloader->downloaded += (uint32_t)bodyLength;

    LOG_TRACE(
        "TMS DL rangeDone(): chunk saved downloaded=%lu/%lu progress=%lu%%",
        (unsigned long)downloader->downloaded,
        (unsigned long)downloader->totalSize,
        (unsigned long)(((uint64_t)downloader->downloaded * 100u) /
                        downloader->totalSize));

    if (downloader->downloaded == downloader->totalSize) {
        LOG_TRACE("TMS DL rangeDone(): all bytes downloaded");
        complete(downloader, true);
        return;
    }

    downloader->pendingRequest = true;
    LOG_TRACE("TMS DL rangeDone(): next range queued (pendingRequest=1)");
}

/* -----------------------------------------------------------
 * Next HTTP range request
 * ----------------------------------------------------------- */

/**
 * @brief Start the next download range request.
 *
 * The Range and Connection header strings are local, matching the
 * existing code. httpFlowStart must consume/copy these header values
 * before returning; it must not retain pointers into this stack frame.
 */
static HttpFlowResult requestNext(TmsDownloader* downloader) {
    HttpFlowHeader headers[2];
    char           rangeValue[64];
    uint32_t       remaining;
    uint32_t       length;
    int            written;
    HttpFlowResult result;

    if (downloader == NULL) {
        LOG_ERROR("TMS DL requestNext(): downloader is NULL");
        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    if (downloader->state != TMS_DOWNLOAD_RUNNING ||
        downloader->downloaded >= downloader->totalSize) {
        LOG_ERROR("TMS DL requestNext(): invalid state/progress state=%d "
                  "downloaded=%lu total=%lu",
                  (int)downloader->state, (unsigned long)downloader->downloaded,
                  (unsigned long)downloader->totalSize);
        return HTTP_FLOW_ERR_REQUEST;
    }

    remaining = downloader->totalSize - downloader->downloaded;
    length    = (remaining < TMS_DOWNLOAD_RANGE_SIZE) ? remaining
                                                      : TMS_DOWNLOAD_RANGE_SIZE;

    if (length == 0u) {
        LOG_ERROR("TMS DL requestNext(): zero-length range");
        return HTTP_FLOW_ERR_REQUEST;
    }

    downloader->requestedStart  = downloader->downloaded;
    downloader->requestedLength = length;

    written =
        snprintf(rangeValue, sizeof(rangeValue), "bytes=%lu-%lu",
                 (unsigned long)downloader->requestedStart,
                 (unsigned long)(downloader->requestedStart + length - 1u));

    /* Check the signed snprintf result before casting to size_t. */
    if (written < 0 || (size_t)written >= sizeof(rangeValue)) {
        LOG_ERROR(
            "TMS DL requestNext(): Range header formatting failed written=%d",
            written);
        return HTTP_FLOW_ERR_REQUEST;
    }

    headers[0].name  = "Range";
    headers[0].value = rangeValue;
    headers[1].name  = "Connection";
    headers[1].value = "close";

    LOG_TRACE("TMS DL requestNext(): GET range=%s remaining=%lu "
              "pathLen=%lu",
              rangeValue, (unsigned long)remaining,
              (unsigned long)strlen(downloader->path));

    result = tmsHttpClientRequest(&downloader->http, HTTP_METHOD_GET,
                                  downloader->path, headers, 2u, NULL, 0u,
                                  rangeDone, downloader);

    if (result != HTTP_FLOW_OK) {
        LOG_ERROR("TMS DL requestNext(): HTTP request start failed "
                  "result=%d range=%s",
                  (int)result, rangeValue);
    } else {
        LOG_TRACE("TMS DL requestNext(): HTTP range request started");
    }

    return result;
}

/* -----------------------------------------------------------
 * Public API
 * ----------------------------------------------------------- */

void tmsDownloaderInit(TmsDownloader*        downloader,
                       const TmsPlatformOps* platform) {
    LOG_TRACE(
        "TMS DL tmsDownloaderInit(): begin downloaderSet=%d platformSet=%d",
        (int)(downloader != NULL), (int)(platform != NULL));

    if (downloader == NULL) {
        LOG_ERROR("TMS DL tmsDownloaderInit(): downloader is NULL");
        return;
    }

    memset(downloader, 0, sizeof(*downloader));
    downloader->platform = platform;

    if (platform == NULL) {
        LOG_ERROR("TMS DL tmsDownloaderInit(): platform is NULL");
    } else {
        LOG_TRACE("TMS DL tmsDownloaderInit(): createFile=%d writeFile=%d "
                  "removeFile=%d",
                  (int)(platform->createFile != NULL),
                  (int)(platform->writeFile != NULL),
                  (int)(platform->removeFile != NULL));
    }

    LOG_TRACE("TMS DL tmsDownloaderInit(): complete");
}

HttpFlowResult tmsDownloaderStart(TmsDownloader* downloader, const char* url,
                                  const char* filePath, uint32_t totalSize,
                                  uint32_t                headerReserve,
                                  TmsDownloadDoneCallback callback,
                                  void*                   userData) {
    HttpFlowResult requestResult;
    size_t         filePathLength;
    int            fileResult;

    LOG_TRACE("TMS DL tmsDownloaderStart(): begin total=%lu headerReserve=%lu "
              "urlSet=%d pathSet=%d",
              (unsigned long)totalSize, (unsigned long)headerReserve,
              (int)(url != NULL), (int)(filePath != NULL));

    if (downloader == NULL || downloader->platform == NULL ||
        downloader->platform->createFile == NULL ||
        downloader->platform->writeFile == NULL || url == NULL ||
        filePath == NULL || totalSize == 0u || callback == NULL) {
        LOG_ERROR("TMS DL tmsDownloaderStart(): invalid arguments "
                  "downloader=%d url=%d filePath=%d total=%lu callback=%d",
                  (int)(downloader != NULL), (int)(url != NULL),
                  (int)(filePath != NULL), (unsigned long)totalSize,
                  (int)(callback != NULL));
        return HTTP_FLOW_ERR_INVALID_ARG;
    }

    /* Avoid clobbering an active download's state and HTTP context. */
    if (downloader->state == TMS_DOWNLOAD_RUNNING) {
        LOG_ERROR("TMS DL tmsDownloaderStart(): downloader already running");
        return HTTP_FLOW_ERR_BUSY;
    }

    if (!parseUrl(url, downloader->host, sizeof(downloader->host),
                  &downloader->port, downloader->path,
                  sizeof(downloader->path))) {
        LOG_ERROR("TMS DL tmsDownloaderStart(): URL parsing failed");
        return HTTP_FLOW_ERR_REQUEST;
    }

    filePathLength = strlen(filePath);
    if (filePathLength >= sizeof(downloader->filePath)) {
        LOG_ERROR(
            "TMS DL tmsDownloaderStart(): file path too long len=%lu cap=%lu",
            (unsigned long)filePathLength,
            (unsigned long)sizeof(downloader->filePath));
        return HTTP_FLOW_ERR_REQUEST;
    }

    memcpy(downloader->filePath, filePath, filePathLength + 1u);
    downloader->totalSize     = totalSize;
    downloader->downloaded    = 0u;
    downloader->headerReserve = headerReserve;
    downloader->onDone        = callback;
    downloader->userData      = userData;

    if (totalSize > UINT32_MAX - headerReserve) {
        LOG_ERROR("TMS DL tmsDownloaderStart(): file size overflow total=%lu "
                  "reserve=%lu",
                  (unsigned long)totalSize, (unsigned long)headerReserve);
        downloader->state = TMS_DOWNLOAD_FAILED;
        return HTTP_FLOW_ERR_REQUEST;
    }

    LOG_TRACE("TMS DL tmsDownloaderStart(): parsed port=%u filePathLen=%lu "
              "allocatedSize=%lu",
              (unsigned int)downloader->port, (unsigned long)filePathLength,
              (unsigned long)(headerReserve + totalSize));

    if (downloader->platform->removeFile != NULL) {
        LOG_TRACE("TMS DL tmsDownloaderStart(): removing existing target file");
        /* removeFile may return void on some platform implementations. */
        downloader->platform->removeFile(filePath);
        LOG_TRACE("TMS DL tmsDownloaderStart(): removeFile invoked");
    }

    LOG_TRACE("TMS DL tmsDownloaderStart(): creating destination file");
    fileResult =
        downloader->platform->createFile(filePath, headerReserve + totalSize);

    if (fileResult != 0) {
        LOG_ERROR("TMS DL tmsDownloaderStart(): createFile failed result=%d "
                  "size=%lu",
                  fileResult, (unsigned long)(headerReserve + totalSize));
        downloader->state = TMS_DOWNLOAD_FAILED;
        return HTTP_FLOW_ERR_NO_RESOURCE;
    }

    LOG_TRACE("TMS DL tmsDownloaderStart(): destination file created");

    tmsHttpClientInit(&downloader->http, downloader->host, downloader->port);
    LOG_TRACE("TMS DL tmsDownloaderStart(): HTTP client initialized");

    downloader->state          = TMS_DOWNLOAD_RUNNING;
    downloader->pendingRequest = false;

    LOG_TRACE("TMS DL tmsDownloaderStart(): starting first range");
    requestResult = requestNext(downloader);

    if (requestResult != HTTP_FLOW_OK) {
        LOG_ERROR(
            "TMS DL tmsDownloaderStart(): first range start failed result=%d",
            (int)requestResult);
        downloader->state = TMS_DOWNLOAD_FAILED;
    } else {
        LOG_TRACE("TMS DL tmsDownloaderStart(): first range start accepted");
    }

    /* A synchronous completion callback may already have changed the state. */
    return requestResult;
}

void tmsDownloaderCancel(TmsDownloader* downloader) {
    LOG_TRACE("TMS DL tmsDownloaderCancel(): requested");

    if (downloader == NULL) {
        LOG_ERROR("TMS DL tmsDownloaderCancel(): downloader is NULL");
        return;
    }

    if (downloader->state != TMS_DOWNLOAD_RUNNING) {
        LOG_TRACE("TMS DL tmsDownloaderCancel(): ignored, state=%d",
                  (int)downloader->state);
        return;
    }

    LOG_TRACE("TMS DL tmsDownloaderCancel(): cancelling at downloaded=%lu/%lu "
              "pending=%d",
              (unsigned long)downloader->downloaded,
              (unsigned long)downloader->totalSize,
              (int)downloader->pendingRequest);

    /* Transition first so synchronous cancellation callbacks are ignored. */
    downloader->state          = TMS_DOWNLOAD_CANCELLED;
    downloader->pendingRequest = false;

    tmsHttpClientCancel(&downloader->http);
    LOG_TRACE("TMS DL tmsDownloaderCancel(): HTTP cancellation issued");
}

void tmsDownloaderProcess(TmsDownloader* downloader) {
    HttpFlowResult result;

    if (downloader == NULL)
        return;

    /* Frequently polled: don't log the normal no-work path. */
    if (downloader->state != TMS_DOWNLOAD_RUNNING ||
        !downloader->pendingRequest)
        return;

    LOG_TRACE("TMS DL tmsDownloaderProcess(): processing queued range "
              "downloaded=%lu/%lu",
              (unsigned long)downloader->downloaded,
              (unsigned long)downloader->totalSize);

    downloader->pendingRequest = false;
    result                     = requestNext(downloader);

    if (result != HTTP_FLOW_OK) {
        LOG_ERROR("TMS DL tmsDownloaderProcess(): next range failed to start "
                  "result=%d",
                  (int)result);
        complete(downloader, false);
    }
}

uint32_t tmsDownloaderProgress(const TmsDownloader* downloader) {
    if (downloader == NULL || downloader->totalSize == 0u)
        return 0u;

    /* No per-call trace: progress may be polled for every UI frame. */
    return (uint32_t)(((uint64_t)downloader->downloaded * 100u) /
                      downloader->totalSize);
}
