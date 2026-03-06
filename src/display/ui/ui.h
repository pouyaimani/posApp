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
    OOP_IMETHOD(void, Menu, setChecked, int);
};
 
OOP_CLASS(Menu) {
    OOP_IMPLEMENTS(Menu);
    lv_obj_t *main;
    lv_obj_t *selector;
    lv_obj_t *checker;
    lv_obj_t *item[MENU_ITEM_MAX];
    int cnt;
    int idx;
    int selected;
};

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

InputBox uiInputBox(lv_obj_t *parent);
Button uiButton(lv_obj_t *parent, unsigned int color, const char * text);
void uiMenu(Menu *, lv_obj_t * parent);
void uiDeleteMenu(Menu *menu);
void uiOnOffMenu(Menu *menu, lv_obj_t * parent);
void uiBar(Bar *bar, lv_obj_t * parent, int min, int max);
void uiBarDelete(Bar *bar);

typedef enum {
    INFO_T_TEXT,
    INFO_T_IMG
} InfoType_t;

OOP_DECLARE_CLASS(InfoPage);

OOP_VTABLE(InfoPage) {
    OOP_IMETHOD(void, InfoPage, setData, InfoType_t, const char *, const char *);
    OOP_IMETHOD(void, InfoPage, show);
    OOP_IMETHOD(void, InfoPage, hide);
};
OOP_CLASS(InfoPage) {
    OOP_IMPLEMENTS(InfoPage);
    InfoType_t type;
    lv_obj_t *title;
    lv_obj_t *body;
    lv_obj_t *img;
    lv_obj_t *line;
};

InfoPage infoPage();

#define SHOW_INFO(title, body)              \
    InfoPage info = infoPage();             \
    OOP_CALL(&info, show);                  \
    OOP_CALL(&info, setData, INFO_T_TEXT,   \
         title, body);

#define HIDE_INFO()                         \
    InfoPage info = infoPage();             \
    OOP_CALL(&info, show);                  \

#endif