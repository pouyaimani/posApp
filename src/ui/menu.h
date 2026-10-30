#ifndef MENU_H_
#define MENU_H_

#include "oop.h"
#include "lvgl.h"
#include "font/myFont.h"
#include "common.h"

typedef enum { MENU_UP, MENU_DOWN } MenuUpDown_t;

typedef struct State State;

/**
 * @brief Callback invoked when a menu item is selected.
 *
 * @param idx      Zero-based index of the selected menu item.
 * @param userData Optional user-defined context.
 */
typedef void (*MenuItemCallback_t)(uint16_t idx, void* userData);

/***************************** Menu *****************************/

#define MENU_ITEM_MAX 20

typedef enum Key_t Key_t;

OOP_CLASS(Menu) {
    lv_obj_t*          main;
    lv_obj_t*          selector;
    lv_obj_t*          checker;
    lv_obj_t*          item[MENU_ITEM_MAX];
    State*             state[MENU_ITEM_MAX];
    void*              userData[MENU_ITEM_MAX];
    MenuItemCallback_t cb[MENU_ITEM_MAX];
    bool               toggle[MENU_ITEM_MAX];
    int                cnt;
    int                idx;
    int                selected;
    bool               checkEnable;
    bool               togglable;
};

int8_t ui_menu_create(Menu*, lv_obj_t* parent);

void ui_menu_destroy(Menu* menu);

void ui_menu_on_off(Menu* menu, lv_obj_t* parent, MenuItemCallback_t cb,
                    void* userData);

void ui_menu_togglable(Menu* menu, lv_obj_t* parent);

void ui_menu_addItem(Menu* menu, const char* text, lv_text_align_t txtAlign,
                     State* state, MenuItemCallback_t cb, void* user_data);

void ui_menu_handleItem(Menu* menu, Key_t key);

void ui_menu_add_on_off_item(Menu* menu, const char* text, bool toggle,
                             State* state, MenuItemCallback_t cb,
                             void* user_data);

void ui_menu_show(Menu* menu);

void ui_menu_set_checked(Menu* menu, int newIdx);

void ui_menu_hide(Menu* menu);

int ui_menu_get_idx(Menu* menu);

void ui_menu_toggle(Menu* menu, int idx);

bool ui_menu_validate(const Menu* menu);

#endif