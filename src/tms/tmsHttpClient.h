#ifndef TMS_HTTP_CLIENT_H
#define TMS_HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "http/httpFlow.h"
#include "tmsUpgradeInfo.h"

#define TMS_HTTP_HOST_MAX_LEN 128u

typedef struct TmsHttpClient TmsHttpClient;

typedef void (*TmsHttpDoneCallback)(TmsHttpClient*      client,
                                    HttpFlowResult      result,
                                    const HttpResponse* response,
                                    const uint8_t* body, size_t bodyLength,
                                    void* userData);

struct TmsHttpClient {
    HttpFlow            flow;
    char                host[TMS_HTTP_HOST_MAX_LEN];
    uint16_t            port;
    TmsHttpDoneCallback onDone;
    void*               userData;
    bool                busy;
};

void tmsHttpClientInit(TmsHttpClient* client, const char* host, uint16_t port);
HttpFlowResult
tmsHttpClientRequest(TmsHttpClient* client, HttpMethod method, const char* path,
                     const HttpFlowHeader* headers, size_t headerCount,
                     const uint8_t* body, size_t bodyLength,
                     TmsHttpDoneCallback callback, void* userData);
HttpFlowResult tmsHttpClientPostJson(TmsHttpClient* client, const char* path,
                                     const char*         json,
                                     TmsHttpDoneCallback callback,
                                     void*               userData);
HttpFlowResult tmsHttpClientGet(TmsHttpClient* client, const char* path,
                                TmsHttpDoneCallback callback, void* userData);
void           tmsHttpClientCancel(TmsHttpClient* client);
void           tmsHttpClientRelease(TmsHttpClient* client);
bool           tmsHttpClientBusy(const TmsHttpClient* client);

#endif
