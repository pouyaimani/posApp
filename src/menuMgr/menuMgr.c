#include "menuMgr.h"

#include "display.h"
#include "keypad/keypadEvent.h"
#include "font/myFont.h"
#include "logger.h"
#include "myColor.h"
#include "mylvgl.h"
#include "states.h"
#include "timeout.h"

static MenuMgr __menuMgr;

/**
 * @brief Convert a numeric keypad key to a zero-based menu item index.
 *
 * A switch is intentionally used instead of arithmetic such as KEY_1 - 1.
 * This avoids depending on the numeric values or contiguity of the key enum.
 *
 * @param key Keypad key value.
 * @param outIndex Destination for the zero-based menu index.
 *
 * @return true when key is KEY_1 .. KEY_9, otherwise false.
 */
static bool keyToMenuIndex(int key, uint16_t* outIndex) {
    RETURN_VALUE_IF_NULL(outIndex, ;, false);

    switch (key) {
    case KEY_1:
        *outIndex = 0U;
        return true;

    case KEY_2:
        *outIndex = 1U;
        return true;

    case KEY_3:
        *outIndex = 2U;
        return true;

    case KEY_4:
        *outIndex = 3U;
        return true;

    case KEY_5:
        *outIndex = 4U;
        return true;

    case KEY_6:
        *outIndex = 5U;
        return true;

    case KEY_7:
        *outIndex = 6U;
        return true;

    case KEY_8:
        *outIndex = 7U;
        return true;

    case KEY_9:
        *outIndex = 8U;
        return true;

    default:
        return false;
    }
}

static void deleteMenu() {
    Menu* menu = __menuMgr.menu;

    /*
     * Invalidate the singleton's active-menu pointer before invoking any UI
     * destruction path.
     */
    __menuMgr.menu = NULL;

    if (menu == NULL) {
        return;
    }

    ui_menu_hide(menu);
    ui_menu_destroy(menu);

    if (__menuMgr.cfg.ownsMenu) {
        MEM_FREE(menu);
    }
}

/**
 * @brief Activate one menu item safely.
 *
 * The function performs all per-item behavior in one place:
 * - optional checked-state update;
 * - optional toggle-state update;
 * - optional callback invocation;
 * - optional state transition.
 *
 * Callback data and the destination state are copied before invoking the
 * callback. This prevents a use-after-free if the callback itself changes
 * state and MenuMgr is exited/destroyed as a result.
 *
 * If the callback performs a state transition, MenuMgr::exit clears the active
 * menu pointer. The final pointer comparison then prevents MenuMgr from issuing
 * a second state transition.
 *
 * @param menu Menu containing the item.
 * @param idx  Zero-based item index.
 */
static void activateItem(Menu* menu, uint16_t idx) {
    RETURN_IF_NULL(menu, ;);

    if (idx >= menu->cnt) {
        return;
    }

    if (menu->checkEnable) {
        ui_menu_set_checked(menu, idx);
    }

    if (menu->togglable) {
        ui_menu_toggle(menu, idx);
    }

    /*
     * Snapshot all data that may be needed after the callback.
     * The callback is allowed to trigger a state transition, which may destroy
     * the currently active menu.
     */
    CallBack_t callback  = menu->cb[idx];
    void*      userData  = menu->userData[idx];
    State*     nextState = menu->state[idx];

    bool activeMenu = (__menuMgr.menu == menu);

    deleteMenu();

    if (callback != NULL) {
        callback(userData);
    }

    /*
     * If the callback changed state, STATE_DEF_EXIT(MenuMgr) clears
     * __menuMgr.menu. Do not override that transition.
     */
    if (activeMenu && (nextState != NULL)) {
        SM_GOTO(nextState);
    }
}

/**
 * @brief Standard timeout handler used by the exported default configs.
 *
 * @param arg Unused.
 */
void back2idle(void* arg) {
    VAR_UNUSED(arg);
    deleteMenu();
    SM_GOTO(STATE_IDLE);
}

/**
 * @brief Default config for dynamically allocated Menu objects.
 */
const MenuMgrCfg menuCfgBack2idleOBM = {
    .timeout   = BACK_IDLE_TMO,
    .onTimeOut = back2idle,
    .onExit    = NULL,
    .userData  = NULL,
    .ownsMenu  = true,
};

/**
 * @brief Default config for Menu objects owned by the caller.
 */
const MenuMgrCfg menuCfgBack2idleOBC = {
    .timeout   = BACK_IDLE_TMO,
    .onTimeOut = back2idle,
    .onExit    = NULL,
    .userData  = NULL,
    .ownsMenu  = false,
};

/**
 * @brief Enter the MenuMgr state.
 *
 * Arms the inactivity timeout and shows the configured menu.
 */
STATE_DEF_ENTER(MenuMgr) {
    if (__menuMgr.menu == NULL) {
        return;
    }
    LOG_TRACE("menu time out duration = %lu", __menuMgr.cfg.timeout);
    OOP_CALL(state, setTimeOut, __menuMgr.cfg.timeout);
    ui_menu_show(__menuMgr.menu);
}

/**
 * @brief Exit the MenuMgr state and release menu resources.
 *
 * The active pointer is cleared before destroying resources. This avoids
 * leaving a dangling singleton pointer and makes callback-driven state changes
 * detectable by activateItem().
 */
STATE_DEF_EXIT(MenuMgr) {}

/**
 * @brief Handle an inactivity timeout.
 *
 * MenuMgr itself does not choose a timeout destination. The configured timeout
 * callback owns that policy.
 */
STATE_DEF_HANDLE(MenuMgr, TimeOutEvent) {
    VAR_UNUSED(ev);

    CallBack_t callback = __menuMgr.cfg.onTimeOut;
    void*      userData = __menuMgr.cfg.userData;

    if (callback != NULL) {
        callback(userData);
    }
}

/**
 * @brief Handle keypad input for the active menu.
 *
 * Behavior:
 * - every keypad event refreshes the inactivity timeout;
 * - menu navigation keys are forwarded to ui_menu_handleItem();
 * - KEY_ENTER activates the currently highlighted item;
 * - KEY_1 .. KEY_9 directly activate the corresponding item when it exists;
 * - KEY_ESC invokes onExit and returns to backTo unless onExit already changed
 *   state.
 */
STATE_DEF_HANDLE(MenuMgr, KeypadEvent) {
    Menu*    menu      = __menuMgr.menu;
    uint16_t itemIndex = 0U;

    if (menu == NULL) {
        return;
    }

    /*
     * BACK_IDLE_TMO is an inactivity timeout, so user activity must restart it.
     */
    OOP_CALL(state, setTimeOut, __menuMgr.cfg.timeout);

    /*
     * Let Menu update its highlighted index for navigation keys first.
     */
    ui_menu_handleItem(menu, ev->key);

    if (ev->key == KEY_ENTER) {
        activateItem(menu, menu->idx);
        return;
    }

    if (keyToMenuIndex(ev->key, &itemIndex)) {
        /*
         * Ignore numeric shortcuts that do not map to an existing item.
         * No menu array is accessed until this bounds check succeeds.
         */
        if (itemIndex < menu->cnt) {
            activateItem(menu, itemIndex);
        }

        return;
    }

    if (ev->key == KEY_ESC) {
        CallBack_t onExit   = __menuMgr.cfg.onExit;
        void*      userData = __menuMgr.cfg.userData;
        State*     backTo   = __menuMgr.backTo;

        bool activeMenu = (__menuMgr.menu == menu);

        deleteMenu();

        if (onExit != NULL) {
            onExit(userData);
        }

        /*
         * The exit callback may perform its own SM_GOTO(). Only apply the
         * configured back transition if this menu is still active afterward.
         */
        if (activeMenu && (backTo != NULL)) {
            SM_GOTO(backTo);
        }
    }
}

/**
 * @brief Start managing a menu.
 *
 * MenuMgr is a singleton; nested/reentrant managed menus are intentionally
 * rejected instead of overwriting the active menu, back-state, and config.
 *
 * @param menu   Menu to manage.
 * @param backTo Destination when KEY_ESC is pressed.
 * @param cfg    Manager configuration.
 */
static void run(Menu* menu, State* backTo, const MenuMgrCfg* cfg) {
    RETURN_IF_NULL(menu, ;);
    RETURN_IF_NULL(cfg, ;);

    if (__menuMgr.menu != NULL) {
        return;
    }

    __menuMgr.menu   = menu;
    __menuMgr.backTo = backTo;
    LOG_TRACE("config time out duration = %lu", cfg->timeout);
    __menuMgr.cfg = *cfg;

    SM_GOTO(&__menuMgr.state);
}

/**
 * @brief Construct a MenuMgr object.
 */
OOP_CTOR(MenuMgr) {
    OOP_CALL_CTOR(State, &self->state, NULL, "menu manager");

    self->state.vtable.enter         = STATE_ENTER(MenuMgr);
    self->state.vtable.exit          = STATE_EXIT(MenuMgr);
    self->state.vtable.handleKeypad  = STATE_HANDLE(MenuMgr, KeypadEvent);
    self->state.vtable.handleTimeout = STATE_HANDLE(MenuMgr, TimeOutEvent);

    self->backTo = NULL;
    self->menu   = NULL;
    self->run    = run;
}

/**
 * @brief Return the lazily constructed singleton MenuMgr.
 */
MenuMgr* menuMgr(void) {
    CALL_ONCE(OOP_CALL_CTOR(MenuMgr, &__menuMgr););
    return &__menuMgr;
}
