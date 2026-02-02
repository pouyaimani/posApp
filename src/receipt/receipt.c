#include "receipt.h"
#include "dev/dev.h"

#define MAX_COLUMN_COUNT    5
#define PRINTER_WIDTH_PIX   320

static lv_obj_t *root;
static Receipt_t receipt;
static lv_img_dsc_t *snapShot;  

typedef enum {
    COMP_TABLE = 0,
    COMP_IMAGE,
    COMP_TEXT,
    COMP_AMOUNT,
    COMP_SPACE,
    COMP_NONE
} ComponentType_t;

ComponentType_t lastInsertedComp = COMP_NONE;

static Receipt_t* addText(int8_t colCount, ...) {
    
    if(colCount == 0 || colCount > MAX_COLUMN_COUNT) return;
    lv_obj_t *row = lv_obj_create(root);
    LV_SET_WIDTH(row, lv_pct(100));
    LV_SET_FLEX_FLOW(row, LV_FLEX_FLOW_ROW_WRAP);
    LV_SET_FLEX_ALIGN(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    LV_HIDE_BORDER(row);
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
        lv_obj_set_style_text_line_space(row, -10, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        LV_SET_PAD_TOP(lbl, -5);
        LV_SET_PAD_BOTTOM(lbl, -5);
        LV_SET_TEXT_COLOR(lbl, 0x000000);
        LV_SET_BG_OPA(lbl, LV_OPA_0);
        LV_HIDE_BORDER(lbl);
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
    va_end(args);
    return &receipt;
}

static Receipt_t* addTable(int8_t colCount, ...) {
    if(colCount == 0 || colCount > MAX_COLUMN_COUNT) return;
    lv_obj_t *row = lv_obj_create(root);
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

static Receipt_t* addImage(int8_t colCount, ...) {  
    if(colCount == 0 || colCount > MAX_COLUMN_COUNT) return;
    lv_obj_t *row = lv_obj_create(root);
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

static Receipt_t* addSpace(uint16_t height) {
    
    lv_obj_t *space = lv_obj_create(root);
    LV_HIDE_BORDER(space);
    LV_SET_WIDTH(space, PRINTER_WIDTH_PIX);
    LV_SET_ALIGN(space, LV_ALIGN_CENTER);
    LV_SET_BG_OPA(space, LV_OPA_0);
    LV_SCROLL_DISABLE(space);
    lastInsertedComp = COMP_SPACE;
    return &receipt;
}

static Receipt_t* addAmount(const char *amount) {
    
    lv_obj_t *lbl = lv_label_create(root);
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

static lv_img_dsc_t* snapshot() {
    snapShot = lv_snapshot_take(root, LV_COLOR_FORMAT_NATIVE);
    return snapShot;
}

static void snpFree() {
    lv_snapshot_free(snapShot);
    lv_obj_del(root);
}

Receipt_t *Receipt() {
    CALL_ONCE(
        receipt.addText = addText;
        receipt.snapshot = snapshot;
        receipt.free = snpFree;
        receipt.addSpace = addSpace;
        receipt.addTable = addTable;
        receipt.addImage = addImage;
    );

    root = lv_obj_create(lv_scr_act());
    receipt.root = root;

    LV_SET_WIDTH(root, PRINTER_WIDTH_PIX);
    LV_ALIGN(root, LV_ALIGN_CENTER, 0, 0);
    LV_HIDE_BORDER(root);
    LV_SET_RADIUS(root, 0);
    LV_SCROLL_DISABLE(root);
    LV_SET_FLEX_FLOW(root, LV_FLEX_FLOW_COLUMN);
    LV_SET_FLEX_ALIGN(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    LV_SET_ROW_PAD(root, -6);
    LV_SET_PAD_ALL(root, -6);
    LV_AUTO_HEIGHT(root);
    LV_HIDE(root);
    lastInsertedComp = COMP_NONE;
}