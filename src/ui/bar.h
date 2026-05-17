#ifndef BAR_H_
#define BAR_H_

#include "oop.h"
#include "lvgl.h"

/***************************** Bar *****************************/

OOP_DECLARE_CLASS(Bar);

OOP_VTABLE(Bar) {
    OOP_IMETHOD(void, Bar, setValue, int);
    OOP_IMETHOD(void, Bar, increase);
    OOP_IMETHOD(void, Bar, decrease);
    OOP_IMETHOD(void, Bar, hide);
    OOP_IMETHOD(void, Bar, show);
    OOP_IMETHOD(void, Bar, setTitle, const char *);
};
 
OOP_CLASS(Bar) {
    OOP_IMPLEMENTS(Bar);
    lv_obj_t *bar;
    lv_obj_t *title;
    int value;
    int max;
    int min;
};

void ui_bar_destroy(Bar *bar);

void ui_bar_create(Bar *bar, lv_obj_t * parent, int min, int max);

#endif