#ifndef UI_H_
#define UI_H_

#include "oop.h"
#include "lvgl.h"
#include "font/myFont.h"

OOP_CLASS(InputBox) {
    lv_obj_t *main;
    lv_obj_t *textBox;
};

OOP_CLASS(Button) {
    lv_obj_t *main;
    lv_obj_t *textBox;
};

typedef enum {
    MENU_UP,
    MENU_DOWN
} MenuUpDown_t;

#define MENU_ITEM_MAX   50

typedef enum Key_t Key_t;

OOP_DECLARE_CLASS(Menu);

OOP_VTABLE(Menu) {
    OOP_IMETHOD(void, Menu, addItem, const char * text,
                lv_event_cb_t event_cb, void * user_data);
    OOP_IMETHOD(void, Menu, handleItem, Key_t);
    OOP_IMETHOD(void, Menu, show);
    OOP_IMETHOD(void, Menu, hide);
    OOP_IMETHOD(int, Menu, getIdx);
};
 
OOP_CLASS(Menu) {
    OOP_IMPLEMENTS(Menu);
    lv_obj_t *main;
    lv_obj_t *item[MENU_ITEM_MAX];
    int cnt;
    int idx;
};

InputBox uiInputBox(lv_obj_t *parent);
Button uiButton(lv_obj_t *parent, unsigned int color, const char * text);
Menu uiMenu(lv_obj_t * parent);

#endif