#include "tmsUpgradeCheck.h"
#include "logger.h"
#include "tmsDownloadProbe.h"

static TmsApi*       api;
static TmsUpgCheckCb callback;

static void checkDone(TmsApi* context, const TmsCheckResult* r, void* user) {
    (void)context;
    (void)user;
    if (callback) {
        callback(r, user);
    }
}

TmsApiResult tmsUpgradeCheckStart(const char* host, uint16_t port,
                                  const char* serial, const char* brand,
                                  const char* current_version, TmsUpgCheckCb cb,
                                  void* udata) {
    TmsCheckRequest request = {0};
    TmsApiResult    result;
    callback                   = cb;
    api                        = MEM_ALLOC(sizeof(TmsApi));
    request.host               = host;
    request.port               = port;
    request.timeout_ms         = 30000;
    request.device_sn          = serial;
    request.brand_code         = brand;
    request.current_version    = current_version;
    request.pack_resource_type = 1;
    request.on_done            = checkDone;
    request.user               = udata;
    result                     = tms_api_check_start(api, &request);
    if (result != TMS_API_OK)
        LOG_TRACE("TMS start rejected: result=%d", (int)result);
    return result;
}

TmsApiResult tmsUpgradeCheckCancel(void) { return tms_api_cancel(api); }
