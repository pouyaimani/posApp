#include "tms.h"
#include "dev/dev.h"
#include "file/file.h"
#include "version.h"
#include "transmitter.h"
#include "storage/storage.h"
#include "settings/settings.h"

static SubState *extractTmsNewVersion;

static void onTmsChunk(uint8_t *data, uint32_t len) {
    // no RAM usage
    // platformWriteFirmwareChunk(data, len);

    // update progress
    // updateProgress(httpCtx->bodyReceived);
}

static void httpBuildContext(const char *ip, const char* path, void (*onBodyChunk)(uint8_t *, uint32_t)) {
    HttpTransmitter *httpTrnas = STATE_TRNS_HTTP;
    HttpContext *httpCtx = httpTrnas->httpCtx;
    memset(httpCtx, 0, sizeof(*httpCtx));

    snprintf(httpCtx->host, sizeof(httpCtx->host), "%s",
             ip);

    snprintf(httpCtx->path, sizeof(httpCtx->path), "%s",
             path);

    httpCtx->rxBuf = GET_MEM(1024);
    httpCtx->rxMax = 1024;

    httpCtx->txBuf = GET_MEM(512);

    // HERE is answer
    httpCtx->rangeStart = 0;
    httpCtx->onBodyChunk = onBodyChunk;
}

static bool isBatteryOk() {
    return OOP_CALL(getDevice(), getBatteryStatus)->level > 
                DEV_BAT_LEV_1 ? true : false;
}

static void getCurrentVersion(TmsVersion *ver) {
    size_t size;
    int ret = OOP_CALL(file(), read, TMS_VERSION_CACHE, ver->data, 0, &size);
    if (strlen(ver->data) == 0 || ret == FILE_ERROR) {
        snprintf(ver->data, size, "%s", TMS_VERSION);
    }
}

static bool isUpdateAvalable() {
    // TmsVersion curVer;
    // TmsVersion newVer;
    // getCurrentVersion(&curVer);
    // getNewVersion(&newVer);
    // return strcmp(curVer.data, newVer.data) == 0 ? false : true;
}

/******************** Extract tms new version sub state **********************/
static uint8_t *versionBuf;
static uint32_t versionLen = 0;

static void fillVersionBuffer(uint8_t *data, uint32_t len) {
    memcpy(versionBuf + versionLen, data, len);
    versionLen += len;
}

STATE_DEF_ENTER(ExtractTmsNewVersion) {

    FREE_MEM(versionBuf);
}

static void ExtractTmsNewVersion(State *parent) {
    extractTmsNewVersion = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, extractTmsNewVersion, parent, "");
    extractTmsNewVersion->vtable.enter = STATE_ENTER(ExtractTmsNewVersion);
}

/*************************** TMS state *******************************/

static void createVersionReq() {
    versionBuf = GET_MEM(512);
    httpBuildContext(settings()->server.mainServerIp, API_CHECK_UPDATES, fillVersionBuffer);
}

STATE_DEF_ENTER(TMS) {
    createVersionReq();
    GOTO_HTTP_TRANSMITTER(state->parent, extractTmsNewVersion);
}

STATE_DEF_EXIT(TMS) {
}

OOP_CTOR(TMS, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(TMS);
    self->base.vtable.exit = STATE_EXIT(TMS);
}