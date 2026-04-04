#ifndef RECEIPT_H_
#define RECEIPT_H_

#include "oop.h"
#include "mylvgl.h"

typedef struct {
    const char *text;
    lv_text_align_t align;
    uint8_t weight;
} Column;

OOP_CLASS(Receipt) {
    OOP_METHOD(Receipt*, addText, int, Column*);
    OOP_METHOD(Receipt*, addBoldText, int culCount, ...);
    OOP_METHOD(Receipt*, addSpace, uint16_t height);
    OOP_METHOD(Receipt*, addTable, int culCount, ...);
    OOP_METHOD(Receipt*, addImage, int culCount, ...);
    OOP_METHOD(Receipt*, addHeader, const char *date, const char *time);
    OOP_METHOD(Receipt*, addFooter);
    OOP_METHOD(Receipt*, addAmount, const char *);
    OOP_METHOD(void, free);
    OOP_METHOD(void, flush);

    lv_draw_buf_t draw_buf;
    uint8_t *buf;
    uint16_t height;
    uint16_t width;
    uint16_t maxHeight;
    lv_obj_t *canvas;
    uint8_t *bitmap;
};

OOP_CTOR(Receipt);

Receipt *createReceipt();

#endif