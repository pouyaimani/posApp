#ifndef RECEIPT_H_
#define RECEIPT_H_

#include "oop.h"
#include "mylvgl.h"

OOP_CLASS(Receipt) {
    lv_obj_t *root;
    OOP_METHOD(Receipt*, addText, int culCount, ...);
    OOP_METHOD(Receipt*, addBoldText, int culCount, ...);
    OOP_METHOD(Receipt*, addSpace, uint16_t height);
    OOP_METHOD(Receipt*, addTable, int culCount, ...);
    OOP_METHOD(Receipt*, addImage, int culCount, ...);
    OOP_METHOD(Receipt*, addHeader, const char *date, const char *time);
    OOP_METHOD(Receipt*, addFooter);
    OOP_METHOD(lv_draw_buf_t*, snapshot);
    OOP_METHOD(void, free);

};

OOP_CTOR(Receipt);

Receipt *createReceipt();

#endif