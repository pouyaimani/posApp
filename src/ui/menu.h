#ifndef MENU_H_
#define MENU_H_

#include "oop.h"
#include "lvgl.h"
#include "font/myFont.h"
#include "common.h"

typedef enum {
    MENU_UP,
    MENU_DOWN
} MenuUpDown_t;

typedef struct State State;

/***************************** Menu *****************************/

#define MENU_ITEM_MAX   20

typedef enum Key_t Key_t;

OOP_DECLARE_CLASS(Menu);

OOP_VTABLE(Menu) {
    OOP_IMETHOD(void, Menu, addItem, const char * text, State *state,
                CallBack_t cb, void * user_data);
    OOP_IMETHOD(void, Menu, addOnOffItem, const char * text, bool onOff, State *state,
                CallBack_t cb, void * user_data);               
    OOP_IMETHOD(void, Menu, handleItem, Key_t);
    OOP_IMETHOD(void, Menu, show);
    OOP_IMETHOD(void, Menu, hide);
    OOP_IMETHOD(int, Menu, getIdx);
    OOP_IMETHOD(void, Menu, setChecked, int);
    OOP_IMETHOD(void, Menu, toggle, int);
};
 
OOP_CLASS(Menu) {
    OOP_IMPLEMENTS(Menu);
    lv_obj_t *main;
    lv_obj_t *selector;
    lv_obj_t *checker;
    lv_obj_t *item[MENU_ITEM_MAX];
    State *state[MENU_ITEM_MAX];
    void *userData[MENU_ITEM_MAX];
    CallBack_t cb[MENU_ITEM_MAX];
    bool toggle[MENU_ITEM_MAX];
    int cnt;
    int idx;
    int selected;
    bool checkEnable;
    bool togglable;
};

void ui_menu_create(Menu *, lv_obj_t * parent);

void ui_menu_destroy(Menu *menu);

void ui_menu_on_off(Menu *menu, lv_obj_t * parent);

void ui_menu_toggle(Menu *menu, lv_obj_t * parent);

#endif