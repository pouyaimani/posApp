#ifndef RECEIPT_H_
#define RECEIPT_H_

#include "oop.h"
#include "mylvgl.h"

OOP_CLASS(Receipt_t) {
    lv_obj_t *root;
    OOP_METHOD(Receipt_t*, addText, int8_t culCount, ...);
    OOP_METHOD(Receipt_t*, addSpace, uint16_t height);
    OOP_METHOD(Receipt_t*, addTable, int8_t culCount, ...);
    OOP_METHOD(Receipt_t*, addImage, int8_t culCount, ...);
    OOP_METHOD(lv_img_dsc_t*, snapshot);
    OOP_METHOD(void, free);

};

Receipt_t *Receipt();

#endif