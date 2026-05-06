#ifndef RECEIPT_H_
#define RECEIPT_H_

#include "oop.h"
#include "mylvgl.h"

#define MAX_CULOMN_CNT      3

typedef struct {
    const char *src;
    int align;
    uint8_t weight;
} Column;

OOP_DECLARE_CLASS(Receipt)
OOP_VTABLE(Receipt) {
    OOP_IMETHOD(uint8_t, Receipt, addText, int, const Column*);
    OOP_IMETHOD(uint8_t, Receipt, addBoldText, int culCount, const Column*);
    OOP_IMETHOD(uint8_t, Receipt, addSpace, uint16_t height);
    OOP_IMETHOD(uint8_t, Receipt, addTable, int culCount, const Column*);
    OOP_IMETHOD(uint8_t, Receipt, addImage, int culCount, const Column*);
    OOP_IMETHOD(uint8_t, Receipt, addHeader, const char *date, const char *time);
    OOP_IMETHOD(uint8_t, Receipt, addFooter);
    OOP_IMETHOD(uint8_t, Receipt, addAmount, const char *);
    OOP_IMETHOD(void, Receipt, free);
    OOP_IMETHOD(void, Receipt, flush);
};

OOP_CLASS(Receipt) {
    OOP_IMPLEMENTS(Receipt);
    lv_draw_buf_t draw_buf;
    uint8_t *buf;
    uint16_t height;
    uint16_t width;
    uint16_t maxHeight;
    lv_obj_t *canvas;
    uint8_t *bitmap;
    char shaped[MAX_CULOMN_CNT][128];
};

bool createReceipt(Receipt* receipt);

#endif