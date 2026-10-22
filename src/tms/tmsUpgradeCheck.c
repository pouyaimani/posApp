#include "tmsUpgradeCheck.h"
#include "logger.h"

static TmsApi* api;

static void checkDone(TmsApi* context, const TmsCheckResult* r, void* user) {
    (void)context;
    (void)user;
    LOG_TRACE("TMS check: result=%d HTTP=%u transport=%d parser=%d",
              (int)r->result, r->http.http_status, (int)r->http.result,
              (int)r->http.http_result);
    if (r->has_server_code)
        LOG_TRACE("TMS server: code=%ld message=%s", (long)r->server_code,
                  r->upgrade.message);
    if (r->result != TMS_API_OK)
        return;
    if (!r->upgrade.has_update) {
        LOG_TRACE("TMS: no update available");
        return;
    }
    LOG_TRACE("TMS: update available version=%s size=%lu", r->upgrade.version,
              (unsigned long)r->upgrade.file_size);
    /* Metadata is available in r->upgrade. No download/install is started. */
}
TmsApiResult tmsUpgradeCheckStart(const char* host, uint16_t port,
                                  const char* serial, const char* brand,
                                  const char* current_version) {
    TmsCheckRequest request = {0};
    TmsApiResult    result;
    api                        = MEM_ALLOC(sizeof(TmsApi));
    request.host               = host;
    request.port               = port;
    request.timeout_ms         = 30000;
    request.device_sn          = serial;
    request.brand_code         = brand;
    request.current_version    = current_version;
    request.pack_resource_type = 1;
    request.on_done            = checkDone;
    result                     = tms_api_check_start(api, &request);
    if (result != TMS_API_OK)
        LOG_TRACE("TMS start rejected: result=%d", (int)result);
    return result;
}
TmsApiResult tmsUpgradeCheckCancel(void) { return tms_api_cancel(api); }
