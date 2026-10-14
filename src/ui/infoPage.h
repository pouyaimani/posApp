#ifndef INFO_PAGE_H_
#define INFO_PAGE_H_

#include "oop.h"
#include "lvgl.h"

#define WAIT_DOT_COUNT 8 #define WAIT_DOT_COUNT 8

typedef enum {
    INFO_SUCCESS,
    INFO_ERROR,
    INFO_WARNING,
    INFO_WAITING,
    INFO_IMG
} InfoType_t;

OOP_CLASS(InfoPage) {
    OOP_METHOD(void, setData, InfoType_t, const char* title, const char* body);
    OOP_METHOD(void, show);
    OOP_METHOD(void, hide);
    OOP_METHOD(void, forceUpdate);
    lv_obj_t*  parent;
    InfoType_t type;
    lv_obj_t*  title;
    lv_obj_t*  body;
    lv_obj_t*  img;
    lv_obj_t*  line;
};

InfoPage* infoPage();

void SHOW_INFO(InfoType_t type, const char* title, const char* body);
void HIDE_INFO();

#endif