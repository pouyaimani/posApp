#ifndef RECEIPT_H_
#define RECEIPT_H_

#include "oop.h"
#include "mylvgl.h"
#include "logger.h"
#include "error.h"

#define MAX_CULOMN_CNT 3

typedef struct {
    const char* src;
    int         align;
    uint8_t     weight;
} RecColumn_t;

typedef enum { REC_FONT_REGULAR, REC_FONT_BOLD } RecFont_t;

OOP_DECLARE_CLASS(Receipt)
OOP_VTABLE(Receipt) {
    OOP_IMETHOD(int8_t, Receipt, addText, RecFont_t font, int,
                const RecColumn_t*);
    OOP_IMETHOD(int8_t, Receipt, addHighlightedText, const char* text,
                RecFont_t font, lv_text_align_t align);
    OOP_IMETHOD(int8_t, Receipt, addSpace, uint16_t height);
    OOP_IMETHOD(int8_t, Receipt, addTable, RecFont_t font, int culCount,
                const RecColumn_t*);
    OOP_IMETHOD(int8_t, Receipt, addImage, int culCount, const RecColumn_t*);
    OOP_IMETHOD(int8_t, Receipt, addTextWithBorder, RecFont_t font,
                int culCount, const RecColumn_t*);
    OOP_IMETHOD(int8_t, Receipt, addHeader, uint32_t date, uint32_t time);
    OOP_IMETHOD(int8_t, Receipt, addFooter);
    OOP_IMETHOD(int8_t, Receipt, addAmount, const char*);
    OOP_IMETHOD(int8_t, Receipt, addLineHorizontal, uint16_t thickness,
                uint16_t paddingTop, uint16_t paddingBottom);
    OOP_IMETHOD(void, Receipt, destroy);
    OOP_IMETHOD(Result_t, Receipt, flush);
};

OOP_CLASS(Receipt) {
    OOP_IMPLEMENTS(Receipt);
    lv_draw_buf_t draw_buf;
    uint8_t*      buf;
    uint16_t      height;
    uint16_t      width;
    uint16_t      maxHeight;
    lv_obj_t*     canvas;
    uint8_t*      bitmap;
    char          shaped[MAX_CULOMN_CNT][128];
};

int8_t createReceipt(Receipt* receipt);

#define RECEIPT_CREATE_ERROR() LOG_ERROR("Receipt: error in creating receipt.")

#endif