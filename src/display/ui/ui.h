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

#define MENU_ITEM_MAX   50

OOP_DECLARE_CLASS(Menu);

OOP_VTABLE(Menu) {
    OOP_IMETHOD(void, Menu, addItem, const char * text,
                lv_event_cb_t event_cb, void * user_data);
};
 
OOP_CLASS(Menu) {
    OOP_IMPLEMENTS(Menu);
    lv_obj_t *main;
    lv_obj_t *item[MENU_ITEM_MAX];
    int cnt;
};

InputBox uiInputBox(lv_obj_t *parent);
Button uiButton(lv_obj_t *parent, unsigned int color, const char * text);
Menu uiMenu(lv_obj_t * parent);

#endif