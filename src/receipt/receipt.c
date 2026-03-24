#include "receipt.h"
#include "dev/dev.h"
#include "storage/storage.h"
#include "display/display.h"
#include "logger.h"
#include "font/myFont.h"

#define MAX_COLUMN_COUNT    5
#define PRINTER_WIDTH_PIX   384
#define MAX_OBJ_HEIGHT      320

static Receipt receipt;
static lv_draw_buf_t snapShotBuff;
static Storage *storage;

typedef enum {
    COMP_TABLE = 0,
    COMP_IMAGE,
    COMP_TEXT,
    COMP_AMOUNT,
    COMP_SPACE,
    COMP_NONE
} ComponentType_t;

ComponentType_t lastInsertedComp = COMP_NONE;

static char *data[5];
static lv_text_align_t align[5];
static int width[5];

static Receipt* addText(int colCount, ...) {
    if(colCount == 0 || colCount > MAX_COLUMN_COUNT) return;
    lv_obj_update_layout(receipt.root);
    lv_obj_t *row = lv_obj_create(receipt.root);
    LV_SET_WIDTH(row, lv_pct(100));
    LV_SET_FLEX_FLOW(row, LV_FLEX_FLOW_ROW_WRAP);
    LV_SET_FLEX_ALIGN(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    LV_HIDE_BORDER(row);
    LV_SCROLL_DISABLE(row);
    LV_AUTO_HEIGHT(row);
    int totalWidth = 0;

    va_list args;
    va_start(args, colCount);
    
    for (int i = 0; i < colCount; i++) {
        data[i] = va_arg(args, char *);
        align[i] = va_arg(args, lv_text_align_t);
        width[i] = va_arg(args, int);
        totalWidth += width[i];
    }
    va_end(args);
    for(uint8_t i = 0; i < colCount; i++) { 
        lv_obj_t *lbl = lv_label_create(row);
        lv_obj_set_style_text_line_space(lbl, -10, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        LV_SET_PAD_TOP(lbl, -5);
        LV_SET_PAD_BOTTOM(lbl, -5);
        LV_SET_TEXT_COLOR(lbl, 0x000000);
        LV_SET_BG_OPA(lbl, LV_OPA_0);
        LV_HIDE_BORDER(lbl);
        LV_SET_TEXT_FONT(lbl, FONT_16);
        LV_SET_TEXT(lbl, data[i]);
        LV_SET_TEXT_ALIGN(lbl, align[i]);
        LV_AUTO_HEIGHT(lbl);
        int unitWidth = lv_pct(100) / totalWidth;
        LV_SET_WIDTH(lbl, unitWidth * width[i]);
        if(i == colCount - 1) {
            LV_SET_TEXT_ALIGN(lbl, LV_TEXT_ALIGN_RIGHT);
        } else {
            LV_SET_TEXT_ALIGN(lbl, LV_TEXT_ALIGN_LEFT);
        }
    }
    lastInsertedComp = COMP_TEXT;
    return &receipt;
}

static Receipt* addBoldText(int8_t colCount, ...) {
    // addText(colCount, ...);
}

static Receipt* addTable(int8_t colCount, ...) {
    if(colCount == 0 || colCount > MAX_COLUMN_COUNT) return;
    lv_obj_t *row = lv_obj_create(receipt.root);
    LV_SET_WIDTH(row, lv_pct(100));
    LV_SET_FLEX_FLOW(row, LV_FLEX_FLOW_ROW_WRAP);
    LV_SET_FLEX_ALIGN(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (lastInsertedComp == COMP_TABLE) {
        LV_HIDE_BORDER_SIDE(row, LV_BORDER_SIDE_TOP);
    }
    LV_SCROLL_DISABLE(row);
    LV_AUTO_HEIGHT(row);
    
    int totalWidth = 0;

    va_list args;
    va_start(args, colCount);

    char *data[colCount];
    lv_text_align_t align[colCount];
    int width[colCount];

    for (int i = 0; i < colCount; i++) {
        data[i] = va_arg(args, char *);
        align[i] = (lv_text_align_t)va_arg(args, int);
        width[i] = 1;
        totalWidth += width[i];
    }

    for(uint8_t i = 0; i < colCount; i++) { 
        lv_obj_t *lbl = lv_label_create(row);
        lv_obj_set_style_text_line_space(row, 0, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        LV_SET_PAD_TOP(lbl, -5);
        LV_SET_PAD_BOTTOM(lbl, -5);
        LV_SET_TEXT_COLOR(lbl, 0);
        LV_SET_BG_OPA(lbl, LV_OPA_0);
        LV_SET_BORDER_WIDTH(lbl, 2);
        LV_SET_BORDER_COLOR(lbl, 0);
        LV_SET_TEXT(lbl, data[i]);
        LV_SET_TEXT_ALIGN(lbl, align[i]);
        LV_AUTO_HEIGHT(lbl);
        int unitWidth = lv_pct(100) / totalWidth;
        LV_SET_WIDTH(lbl, unitWidth * width[i]);

        if(i == 0) {
            LV_HIDE_BORDER(lbl);
        } else {
            LV_HIDE_BORDER_SIDE(lbl, LV_BORDER_SIDE_LEFT);
        }
    }

    va_end(args);
    lastInsertedComp = COMP_TABLE;
    return &receipt;
}

static Receipt* addImage(int8_t colCount, ...) {  
    if(colCount == 0 || colCount > MAX_COLUMN_COUNT) return;
    lv_obj_t *row = lv_obj_create(receipt.root);
    LV_SET_WIDTH(row, lv_pct(100));
    LV_SET_FLEX_FLOW(row, LV_FLEX_FLOW_ROW_WRAP);
    LV_SET_FLEX_ALIGN(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    LV_HIDE_BORDER(row);
    LV_SCROLL_DISABLE(row);
    LV_AUTO_HEIGHT(row);
    
    va_list args;
    va_start(args, colCount);

    char *data[colCount];
    lv_align_t align[colCount];

    for(uint8_t i = 0; i < colCount; i++) { 
        data[i] = va_arg(args, char *);
        align[i] = (lv_align_t)va_arg(args, int);
        lv_obj_t *img = lv_img_create(row);
        lv_img_set_src(img, data[i]);
    }

    va_end(args);
    lastInsertedComp = COMP_TABLE;
    return &receipt;
}

static Receipt* addSpace(uint16_t height) {
    
    lv_obj_t *space = lv_obj_create(receipt.root);
    LV_HIDE_BORDER(space);
    LV_SET_WIDTH(space, PRINTER_WIDTH_PIX);
    LV_SET_ALIGN(space, LV_ALIGN_CENTER);
    LV_SET_BG_OPA(space, LV_OPA_0);
    LV_SCROLL_DISABLE(space);
    lastInsertedComp = COMP_SPACE;
    return &receipt;
}

static Receipt* addAmount(const char *amount) {
    
    lv_obj_t *lbl = lv_label_create(receipt.root);
    LV_HIDE_BORDER(lbl);
    LV_SET_WIDTH(lbl, PRINTER_WIDTH_PIX);
    LV_SET_ALIGN(lbl, LV_ALIGN_CENTER);
    LV_SET_BG_COLOR(lbl, 0);
    LV_SET_TEXT_COLOR(lbl, 0xFFFFFF);
    LV_SCROLL_DISABLE(lbl);
    LV_SET_PAD_TOP(lbl, -10);
    LV_SET_PAD_BOTTOM(lbl, -10);

    char str[64];
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str),
         " مبلغ: %s ریال ",
         amount);
    LV_SET_TEXT(lbl, str);
    lastInsertedComp = COMP_SPACE;
    return &receipt;
}

static Receipt* addHeader(const char *date, const char *time) {
    TerminalSettings *t = &storage->settings->terminal;
    addText(2, t->merchantNo, LV_TEXT_ALIGN_LEFT, 1,
        t->merchantName, LV_TEXT_ALIGN_RIGHT, 1);
    char dataTimeTerm[64] = {0};
    snprintf(dataTimeTerm, sizeof(dataTimeTerm), "%s - %s / %s", date, time, t->terminalNo);
    addText(2, dataTimeTerm, LV_TEXT_ALIGN_LEFT, 2,
        " پایانه / زمان", LV_TEXT_ALIGN_RIGHT, 1);
    return &receipt;
}

static Receipt* addFooter() {
    return &receipt;
}

static lv_draw_buf_t * snapshot() {
    int size = (PRINTER_WIDTH_PIX / 8) * MAX_OBJ_HEIGHT;
    uint8_t *buf = GET_MEM(size);
    lv_draw_buf_init(&snapShotBuff, PRINTER_WIDTH_PIX / 8, MAX_OBJ_HEIGHT, LV_COLOR_FORMAT_I1, 0, buf, size);
    lv_res_t res = lv_snapshot_take_to_draw_buf(receipt.root, LV_COLOR_FORMAT_I1, &snapShotBuff);
    LOG_ERROR("snapshot res = %d", res);
    return res == LV_RESULT_OK ? &snapShotBuff : NULL;
}

static void snpFree() {
    FREE_MEM(snapShotBuff.data);
    lv_obj_del(receipt.root);
}

OOP_CTOR(Receipt) {
    receipt.addText = addText;
    receipt.addSpace = addSpace;
    receipt.addTable = addTable;
    receipt.addImage = addImage;
    receipt.addHeader = addHeader;
    receipt.addFooter = addFooter;
    receipt.addBoldText = addBoldText;
    receipt.snapshot = snapshot;
    receipt.free = snpFree;
    storage = getStorage();
}

Receipt *createReceipt() {
    CALL_ONCE(
        OOP_CALL_CTOR(Receipt, &receipt);
    );

    receipt.root = lv_obj_create(getDisplay()->screen);
    if (!lv_obj_is_valid(receipt.root)) {
            LOG_ERROR("receipt.root is NULL!");
            return NULL;
    }

    LV_SET_WIDTH(receipt.root, PRINTER_WIDTH_PIX);
    LV_ALIGN(receipt.root, LV_ALIGN_CENTER, 0, 0);
    LV_HIDE_BORDER(receipt.root);
    LV_SET_RADIUS(receipt.root, 0);
    LV_SCROLL_DISABLE(receipt.root);
    LV_SET_FLEX_FLOW(receipt.root, LV_FLEX_FLOW_COLUMN);
    LV_SET_FLEX_ALIGN(receipt.root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    LV_SET_ROW_PAD(receipt.root, -6);
    LV_SET_PAD_ALL(receipt.root, -6);
    LV_AUTO_HEIGHT(receipt.root);
    LV_HIDE(receipt.root);
    lastInsertedComp = COMP_NONE;
    return &receipt;
}