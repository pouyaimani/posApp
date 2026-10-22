#ifndef TMS_API_H
#define TMS_API_H
#include "http/httpClient.h"

typedef enum {
    TMS_API_OK              = 0, /* Check completed; inspect has_update. */
    TMS_API_INVALID         = -1,
    TMS_API_BUSY            = -2,
    TMS_API_JSON_ERROR      = -3,
    TMS_API_SCHEMA_ERROR    = -4,
    TMS_API_LIMIT           = -5,
    TMS_API_HTTP_ERROR      = -6,
    TMS_API_SERVER_ERROR    = -7,
    TMS_API_TRANSPORT_ERROR = -8,
    TMS_API_CANCELLED       = -9
} TmsApiResult;

typedef struct {
    bool     has_update;
    uint32_t file_size;
    char     version[64];
    char     url[512];
    char     hash[129]; /* Opaque server hash; algorithm not assumed. */
    char     plan_id[64];
    char     timestamp[32];
    char     message[128];
} TmsUpgradeInfo;

typedef struct {
    TmsApiResult     result;
    HttpClientStatus http;
    bool             has_server_code;
    int32_t          server_code;
    TmsUpgradeInfo   upgrade; /* Use upgrade metadata only when result == OK. */
} TmsCheckResult;

typedef struct TmsApi TmsApi;
/* Called once for an accepted check. Result remains valid until next start.
 * Queue the next step; do not start/cancel/reset this object in callbacks. */
typedef void (*TmsCheckDone)(TmsApi* api, const TmsCheckResult* result,
                             void* user);

typedef struct {
    const char* host; /* NTH host/IP, without scheme or port. */
    uint16_t    port; /* Plain HTTP port used by your existing server. */
    uint32_t    timeout_ms;
    const char* device_sn;
    const char* brand_code;
    const char*
        current_version;    /* Already formatted, e.g. V + PNA_TMS_VERSION. */
    int pack_resource_type; /* Legacy application update uses 1. */
    TmsCheckDone on_done;
    void*        user;
} TmsCheckRequest;

/* Zero-initialize once (static TmsApi api). One check at a time, owner task
 * only. Header and JSON buffers are fixed; cJSON uses your configured
 * allocation hooks temporarily. This object must outlive the asynchronous
 * operation. Request strings are copied by start; they need not persist
 * afterward. Keep these layouts consistent in every compilation unit. */
struct TmsApi {
    HttpClient     client;
    char           header_storage[1024];
    char           json[1536]; /* Reused: request JSON, then response JSON. */
    size_t         received;
    TmsCheckResult result;
    TmsApiResult   receive_error;
    TmsCheckDone   on_done;
    void*          user;
    bool           busy;
    bool           in_callback;
};

/* OK = asynchronous check accepted. Rejection returns synchronously with no
 * callback; api->result carries details except for INVALID/BUSY arguments.
 * No update is a successful check with upgrade.has_update == false. */
TmsApiResult tms_api_check_start(TmsApi* api, const TmsCheckRequest* request);
TmsApiResult tms_api_cancel(TmsApi* api);
#endif
