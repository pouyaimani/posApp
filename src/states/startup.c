#include "states.h"
#include "logger.h"
#include "eventAll.h"
#include "sys/sys.h"
#include "display.h"
#include "keypad/keypad.h"
#include "touchpad/touchpad.h"
#include "magReader/magReader.h"
#include "eventloop.h"
#include "timer.h"
#include "wifi/wifi.h"
#include "storage/storage.h"
#include "network/network.h"
#include "settings/settings.h"
#include "record/shiftRecs.h"
#include "nth/nth.h"
#include "error.h"
#include "record/txnRecs.h"
#include "file/file.h"
#include "wifiPwdMgr/wifiPwdMgr.h"
#include "wifiPwdMgr/wifiAutoConnector.h"

static lv_obj_t* startUpPage;
static lv_obj_t* label;

static void initWifiPwdMgr(void) {
    WifiPwdMgr* mgr = wifiPwdMgr();

    if (mgr->isReady()) {
        LOG_TRACE("WifiPwdMgr already initialized");
        return;
    }

    WifiPmResult result = mgr->init(WIFI_PM_STORAGE_SLOTS);

    if (result != WIFI_PM_OK) {
        LOG_ERROR("WifiPwdMgr initialization failed: %d", (int)result);
        return;
    }

    LOG_TRACE("WifiPwdMgr initialized successfully");
    LOG_TRACE("WifiPwdMgr stored networks: %u", (unsigned)mgr->count());
}

static bool initWifiAutoConnector(void) {
    WifiAutoConnector* mgr = wifiAutoConnector();

    /* Prevent repeated initialization. */
    if (mgr->isReady()) {
        LOG_TRACE("WifiAutoConnector already initialized");
        return true;
    }

    /* Password manager must be initialized first. */
    if (!wifiPwdMgr()->isReady()) {
        LOG_ERROR("WifiAutoConnector: WifiPwdMgr not ready");
        return false;
    }

    /* Bind the connector to the existing Wi-Fi HAL. */
    WifiAutoPort port;
    wifiAutoPortFill(&port);

    if (!mgr->init(&port)) {
        LOG_ERROR("WifiAutoConnector initialization failed");
        return false;
    }

    LOG_TRACE("WifiAutoConnector initialized successfully");
    LOG_TRACE("WifiAutoConnector stage=%d", (int)mgr->getStage());

    return true;
}

STATE_DEF_ENTER(Startup) {
    KEYPAD_INIT();
    MAG_INIT();
    WIFI_INIT();
    TOUCH_INIT();
    // settings()->reset();
    settings()->load();
    disp()->init();
    nth()->init();
    Core* core = smCore();
    core->registerCallback(disp()->update);
    core->registerCallback(getEventloop()->runCycle);
    core->registerCallback(getTimerHanlder()->runCycle);
    core->registerCallback(nth()->tick);

    // Network setitings
    Result_t res = network()->init(settings()->terminal.netRoute);
    if (res.err != ERR_DSC_OK) {
        LOG_DEBUG("Network init error = %d", res.err);
    }

    initWifiPwdMgr();

    /* Initialize Wi-Fi automatic connection controller. */
    if (!initWifiAutoConnector()) {
        LOG_ERROR("WiFi auto connector is unavailable");
    }

    LOG_TRACE("Free space in flash = %lu", OOP_CALL(file(), getFlashFreeSpace));

    // shifts()->init();

    // txnrecord()->init();

    LOG_DEBUG("device voulme = %d", settings()->terminal.devVolume);
    OOP_CALL(sys(), setVolume, settings()->terminal.devVolume);
    OOP_CALL(sys(), setBrightness, settings()->terminal.brightness);
    statusBar();
    GOTO_IDLE();
}

STATE_DEF_EXIT(Startup) {}

STATE_DEF_HANDLE(Startup, TimeOutEvent) {}

OOP_CTOR(Startup, State* parent, const char* name) {
    LOG_TRACE("Constructing Startup ...");
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter         = STATE_ENTER(Startup);
    self->base.vtable.exit          = STATE_EXIT(Startup);
    self->base.vtable.handleTimeout = STATE_HANDLE(Startup, TimeOutEvent);
}