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

typedef struct {
    lv_color_t color;
    const char *symbol;
    const char *title;
} InfoTheme_t;

typedef struct {
    lv_obj_t *circle;
    lv_obj_t *dot;
} OrbitCtx;

typedef enum {
    INFO_STATE_HIDDEN,
    INFO_STATE_WAITING,
    INFO_STATE_SUCCESS,
    INFO_STATE_ERROR,
    INFO_STATE_WARNING,
    INFO_STATE_IMAGE
} InfoState;

typedef struct {

    lv_obj_t *ripple1;
    lv_obj_t *ripple2;

    lv_obj_t *circle;

    lv_obj_t *icon;

    lv_obj_t *orbit_dot;

    lv_obj_t *title;

    OrbitCtx orbit;

} InfoAnim_t;

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
    InfoAnim_t anim;
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