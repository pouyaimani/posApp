#ifndef INFO_PAGE_H_
#define INFO_PAGE_H_

#include "oop.h"
#include "lvgl.h"

#define WAIT_DOT_COUNT 8#define WAIT_DOT_COUNT 8

typedef enum {
    INFO_SUCCESS,
    INFO_ERROR,
    INFO_WARNING,
    INFO_WAITING,
    INFO_IMG
} InfoType_t;

OOP_DECLARE_CLASS(InfoPage);

OOP_VTABLE(InfoPage) {
    OOP_IMETHOD(void, InfoPage, setData, InfoType_t, const char *, const char *);
    OOP_IMETHOD(void, InfoPage, show);
    OOP_IMETHOD(void, InfoPage, hide);
};
OOP_CLASS(InfoPage) {
    OOP_IMPLEMENTS(InfoPage);
    lv_obj_t *parent;
    InfoType_t type;
    lv_obj_t *title;
    lv_obj_t *body;
    lv_obj_t *img;
    lv_obj_t *line;
};

InfoPage *infoPage();

#define SHOW_INFO(type, title, body)            \
    do {                                        \
        InfoPage *info = infoPage();            \
        OOP_CALL(info, setData, (InfoType_t)type,    \
             title, body);                      \
        OOP_CALL(info, show);                   \
    } while (0)

#define HIDE_INFO()                         \
    do {                                    \
        InfoPage *info = infoPage();         \
        OOP_CALL(info, hide);               \
    } while(0)

#endif