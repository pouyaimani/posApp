#include "tms.h"
#include "sys/sys.h"
#include "file/file.h"
#include "version.h"
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
}

static bool isBatteryOk() {
    return OOP_CALL(sys(), getBatteryStatus)->level > 
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

    MEM_FREE(versionBuf);
}

static void ExtractTmsNewVersion(State *parent) {
    extractTmsNewVersion = (SubState *)MEM_ALLOC(sizeof(SubState));
    OOP_CALL_CTOR(State, extractTmsNewVersion, parent, "");
    extractTmsNewVersion->vtable.enter = STATE_ENTER(ExtractTmsNewVersion);
}

/*************************** TMS state *******************************/

static void createVersionReq() {
}

STATE_DEF_ENTER(TMS) {
}

STATE_DEF_EXIT(TMS) {
}

OOP_CTOR(TMS, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(TMS);
    self->base.vtable.exit = STATE_EXIT(TMS);
}