#ifndef MENU_MGR_H_
#define MENU_MGR_H_

#include <stdbool.h>
#include <stdint.h>

#include "common.h"
#include "state.h"
#include "ui/menu.h"

/**
 * @file menuMgr.h
 * @brief Generic state-machine based menu controller.
 *
 * MenuMgr owns the interaction policy around a Menu:
 * - shows the menu when its state is entered;
 * - forwards keypad events to the menu;
 * - activates the currently selected item on KEY_ENTER;
 * - supports direct item selection through KEY_1 .. KEY_9;
 * - invokes optional timeout and exit callbacks;
 * - returns to a configured state on KEY_ESC;
 * - destroys the menu UI when leaving the manager state;
 * - optionally frees the Menu structure itself.
 *
 * @note Menu item callbacks always receive the corresponding
 *       Menu::userData[index]. A NULL userData remains NULL.
 */

/**
 * @brief Runtime configuration for MenuMgr.
 *
 * Ownership rule:
 * - ownsMenu == false:
 *   MenuMgr destroys the UI resources through ui_menu_destroy(), but does not
 *   free the Menu structure.
 *
 * - ownsMenu == true:
 *   MenuMgr destroys the UI resources and then frees the Menu structure using
 *   MEM_FREE().
 *
 * The caller must not free or destroy an active menu while MenuMgr is using it.
 */
typedef struct {
    /**
     * @brief Inactivity timeout passed to State::setTimeOut().
     *
     * The timeout is armed when the menu is entered and refreshed whenever a
     * keypad event is received.
     */
    uint32_t timeout;

    /**
     * @brief Optional callback invoked when the menu timeout expires.
     *
     * Receives userData.
     */
    CallBack_t onTimeOut;

    /**
     * @brief Optional callback invoked when KEY_ESC is pressed.
     *
     * Receives userData.
     *
     * The callback may perform its own state transition. In that case,
     * MenuMgr will not subsequently force the configured backTo transition.
     */
    CallBack_t onExit;

    /**
     * @brief Opaque data passed to onTimeOut and onExit.
     */
    void* userData;

    /**
     * @brief True if MenuMgr owns the Menu allocation.
     *
     * When true, the Menu object is released with MEM_FREE() during state exit.
     */
    bool ownsMenu;
} MenuMgrCfg;

/**
 * @brief State-machine wrapper around a Menu.
 *
 * MenuMgr is implemented as a singleton. Only one managed menu may therefore
 * be active at a time.
 */
OOP_CLASS(MenuMgr) {
    State      state;  /**< State-machine state owned by MenuMgr. */
    State*     backTo; /**< State entered when KEY_ESC is pressed. */
    Menu*      menu;   /**< Currently active menu, or NULL when inactive. */
    MenuMgrCfg cfg;    /**< Copy of the active menu configuration. */

    /**
     * @brief Run a menu through the menu manager.
     *
     * @param menu   Menu to show. Must not be NULL.
     * @param backTo State entered on KEY_ESC. Must not be NULL.
     * @param cfg    Menu manager configuration. Must not be NULL.
     *
     * @note The configuration is copied internally, so cfg only needs to remain
     *       valid during this call.
     *
     * @note Because MenuMgr is a singleton, calling run() while another managed
     *       menu is active is rejected.
     */
    void (*run)(Menu* menu, State* backTo, const MenuMgrCfg* cfg);
};

/**
 * @brief Get the singleton MenuMgr instance.
 *
 * The object is constructed on first use.
 *
 * @return Pointer to the singleton MenuMgr.
 */
MenuMgr* menuMgr(void);

/**
 * @brief Standard timeout callback that returns the state machine to
 * STATE_IDLE.
 *
 * @param arg Unused.
 */
void back2idle(void* arg);

/**
 * @brief 30-second back-to-idle configuration for heap-owned menus.
 *
 * MenuMgr will free the Menu structure when leaving the manager state.
 */
extern const MenuMgrCfg menuCfgBack2idleOBM;

/**
 * @brief 30-second back-to-idle configuration for externally owned menus.
 *
 * MenuMgr destroys the menu UI but does not free the Menu structure.
 */
extern const MenuMgrCfg menuCfgBack2idleOBC;

#endif /* MENU_MGR_H_ */
