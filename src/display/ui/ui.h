#ifndef UI_H_
#define UI_H_

#include "oop.h"
#include "lvgl.h"

OOP_CLASS(InputBox) {
    lv_obj_t *main;
    lv_obj_t *textBox;
};

InputBox uiInputBox(lv_obj_t *parent);
lv_obj_t *uiConfirmButton(lv_obj_t *parent);
lv_obj_t *uiCancellButton(lv_obj_t *parent);

lv_obj_t *uiMenu(lv_obj_t * parent);
lv_obj_t *uiMenuAddItem(lv_obj_t * menu,
                         const char * text,
                             lv_event_cb_t event_cb,
                                 void * user_data);



#endif