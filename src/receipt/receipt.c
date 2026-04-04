#include "receipt.h"
#include "lvgl.h"
#include "dev/dev.h"
#include "storage/storage.h"
#include "logger.h"
#include "font/myFont.h"
#include "display/display.h"
#include "printer/printer.h"

#define PRINTER_WIDTH_PIX   384
#define MAX_HEIGHT          100   // dynamic safe max
#define MAX_CULOMN_CNT      5

char *shaped[MAX_CULOMN_CNT];

static Receipt receipt;
static Storage *storage;

/* =========================
   Internal Helpers
   ========================= */

static void flushReceipt() {
    // Send current buffer to printer
    getPrinter()->print(receipt.bitmap,
        receipt.width, receipt.height);

    // Clear canvas
    lv_canvas_fill_bg(receipt.canvas, lv_color_white(), LV_OPA_COVER);

    // Reset cursor
    receipt.height = 0;
}

static void flushIfNeeded(Receipt *r, uint16_t next_h) {
    if (r->height + next_h > r->maxHeight) {
        flushReceipt(r);
    }
}

static void draw_text_line(lv_layer_t *layer,
                           int x, int y, int w, int height, 
                           const char *txt,
                           lv_text_align_t align,
                           const lv_font_t *font) {
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = lv_color_black();
    dsc.flag = 0;
    dsc.bidi_dir = LV_BASE_DIR_AUTO;
    dsc.font = font;
    dsc.align = align;
    dsc.text = txt;

    lv_area_t coords = {
        .x1 = x,
        .y1 = y,
        .x2 = x + w - 1,
        .y2 = y + height - 1
    };

    lv_draw_label(layer, &dsc, &coords);
}

static uint16_t measure_text_height(const char *txt, int width, const lv_font_t *font) {
    lv_point_t size;
    char tmp[256];
    lv_text_ap_proc(txt, tmp);
    lv_text_get_size(&size, tmp, font, 0, 0, width, LV_TEXT_FLAG_NONE);
    return size.y;
}

static Receipt* addText(int count, Column *cols) {
    Receipt *r = &receipt;
    int totalWeight = 0;
    for(int i = 0; i < count; i++) totalWeight += cols[i].weight;

    uint16_t height = 0;

    for(int i = 0; i < count; i++) {
        int w = (PRINTER_WIDTH_PIX * cols[i].weight) / totalWeight;

        uint16_t h = measure_text_height(cols[i].text, w, &FONT_16);
        if(h > height) height = h;
    }
    flushIfNeeded(r, height);

    int x = 0;

    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);
    for(int i = 0; i < count; i++) {
        int w = (PRINTER_WIDTH_PIX * cols[i].weight) / totalWeight;
        lv_text_ap_proc(cols[i].text, shaped[i]);
        draw_text_line(&layer,
                       x,
                       r->height,
                       w,
                       height,
                       shaped[i],
                       cols[i].align,
                       &FONT_16);

        x += w;
    }
    lv_canvas_finish_layer(r->canvas, &layer);
    r->height += height;
    return r;
}

static Receipt* addTable(int count, Column *cols) {
    Receipt *r = &receipt;

    int totalWeight = count;   // equal width columns
    int x = 0;
    uint16_t max_h = 0;

    // Calculate row height
    for(int i = 0; i < count; i++) {
        int w = PRINTER_WIDTH_PIX / totalWeight;
        uint16_t h = measure_text_height(cols[i].text, w - 4, &FONT_16);
        if(h > max_h) max_h = h;
    }

    // Add some padding
    max_h += 6;

    // Flush if needed
    flushIfNeeded(r, max_h);

    // Start drawing
    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);

    int row_y1 = r->height;
    int row_y2 = r->height + max_h;

    // --- Draw outer horizontal lines ---
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_black();
    line.width = 1;

    // Top border
    line.p1.x = 0;
    line.p1.y = row_y1;
    line.p2.x = PRINTER_WIDTH_PIX;
    line.p2.y = row_y1;
    lv_draw_line(&layer, &line);

    // Bottom border
    line.p1.y = row_y2;
    line.p2.y = row_y2;
    lv_draw_line(&layer, &line);

    // Draw vertical lines + text ---
    x = 0;
    for(int i = 0; i < count; i++) {
        int w = PRINTER_WIDTH_PIX / totalWeight;

        // Draw vertical line (left border + separators)
        line.p1.x = x;
        line.p1.y = row_y1;
        line.p2.x = x;
        line.p2.y = row_y2;
        lv_draw_line(&layer, &line);

        // Draw text (with padding)
        draw_text_line(&layer,
                       x + 2,
                       r->height + 2,
                       w - 4,
                       max_h,
                       cols[i].text,
                       cols[i].align,
                       &FONT_16);

        x += w;
    }

    // Rightmost border
    line.p1.x = PRINTER_WIDTH_PIX - 1;
    line.p1.y = row_y1;
    line.p2.x = PRINTER_WIDTH_PIX - 1;
    line.p2.y = row_y2;
    lv_draw_line(&layer, &line);

    lv_canvas_finish_layer(r->canvas, &layer);

    r->height += max_h;

    return r;
}

/* -------- IMAGE -------- */
static Receipt* addImage(const void *src) {
    Receipt *r = &receipt;
    flushReceipt(r);
    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);

    lv_draw_image_dsc_t dsc;
    lv_draw_image_dsc_init(&dsc);

    lv_area_t coords = {
        .x1 = 0,
        .y1 = r->height,
        .x2 = PRINTER_WIDTH_PIX,
        .y2 = r->height + 100
    };
    dsc.src = src;
    lv_draw_image(&layer, &dsc, &coords);
    lv_canvas_finish_layer(r->canvas, &layer);
    r->height += 100; // TODO: dynamic size if needed
    return r;
}

/* -------- SPACE -------- */
static Receipt* addSpace(uint16_t h) {
    Receipt *r = &receipt;
    flushIfNeeded(r, h);
    r->height += h;
    return r;
}

static Receipt* addHighlightedText(const char *text,
                                   const lv_font_t *font,
                                   lv_text_align_t align)
{
    Receipt *r = &receipt;

    if (!font) font = &FONT_16;

    // Measure text ---
    uint16_t text_h = measure_text_height(text, PRINTER_WIDTH_PIX, font);

    uint16_t padding_top = 4;
    uint16_t padding_bottom = 4;
    uint16_t height = text_h + padding_top + padding_bottom;

    // Flush if needed ---
    flushIfNeeded(r, height);

    // Start drawing ---
    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);

    // Draw BLACK background ---
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_black();
    rect_dsc.bg_opa = LV_OPA_COVER;

    lv_area_t bg = {
        .x1 = 0,
        .y1 = r->height,
        .x2 = PRINTER_WIDTH_PIX - 1,
        .y2 = r->height + height - 1
    };

    lv_draw_rect(&layer, &rect_dsc, &bg);

    // Draw WHITE text ---
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_white();
    label_dsc.font = font;
    label_dsc.align = align;
    label_dsc.flag |= LV_TEXT_FLAG_EXPAND;
    label_dsc.bidi_dir = LV_BASE_DIR_AUTO;
    char tmp[256];
    lv_text_ap_proc(text, tmp);
    label_dsc.text = tmp;

    lv_area_t txt_area = {
        .x1 = 0,
        .y1 = r->height + padding_top,
        .x2 = PRINTER_WIDTH_PIX - 1,
        .y2 = r->height + padding_top + text_h - 1
    };

    lv_draw_label(&layer, &label_dsc, &txt_area);

    // Finish ---
    lv_canvas_finish_layer(r->canvas, &layer);

    r->height += height;

    return r;
}

static Receipt* addAmount(const char *amount) {
    char buf[64];
    snprintf(buf, sizeof(buf), " مبلغ: %s ریال ", amount);

    return addHighlightedText(buf, &FONT_16, LV_TEXT_ALIGN_CENTER);
}

/* -------- HEADER -------- */
static Receipt* addHeader(const char *date, const char *time) {
    Receipt *r = &receipt;
    TerminalSettings *t = &storage->settings->terminal;

    Column row1[] = {
        {t->merchantNo, LV_TEXT_ALIGN_LEFT, 1},
        {t->merchantName, LV_TEXT_ALIGN_RIGHT, 1}
    };

    addText(2, row1);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s - %s / %s",
             date, time, t->terminalNo);

    Column row2[] = {
        {buf, LV_TEXT_ALIGN_LEFT, 2},
        {" پایانه / زمان", LV_TEXT_ALIGN_RIGHT, 1}
    };

    addText(2, row2);

    return r;
}

/* -------- FOOTER -------- */
static Receipt* addFooter()
{
    return &receipt;
}

static void freeReceipt() {
    Receipt *r = &receipt;
    if (r->canvas) {
        LV_DELETE(r->canvas);
        r->canvas = NULL;
    }
    if(r->buf) {
        FREE_MEM(r->buf);
        r->buf = NULL;
    }

    for (size_t i = 0; i < MAX_CULOMN_CNT; i++) {
        FREE_MEM(shaped[i]);
    }
}

OOP_CTOR(Receipt) {
    self->addText = addText;
    self->addSpace = addSpace;
    self->addTable = addTable;
    self->addImage = addImage;
    self->addHeader = addHeader;
    self->addFooter = addFooter;
    // self->addBoldText = addBoldText;
    self->free = freeReceipt;
    self->flush = flushReceipt;
    self->addAmount = addAmount;

    for (size_t i = 0; i < MAX_CULOMN_CNT; i++) {
        shaped[i] = GET_MEM(256);
    }

    receipt.maxHeight = MAX_HEIGHT;
    receipt.height = 0;
    receipt.width = PRINTER_WIDTH_PIX;
    uint32_t stride = (PRINTER_WIDTH_PIX + 7) / 8;  // bytes per row
    uint32_t paletteSize =
        LV_COLOR_INDEXED_PALETTE_SIZE(LV_COLOR_FORMAT_I1) * sizeof(lv_color32_t);

    uint32_t bufSize = stride * MAX_HEIGHT + LV_DRAW_BUF_ALIGN + 
        paletteSize;
    receipt.buf = GET_MEM(bufSize);

    if(!receipt.buf) {
        return NULL;
    }

    lv_draw_buf_init(&receipt.draw_buf,
                     PRINTER_WIDTH_PIX,
                     MAX_HEIGHT,
                     LV_COLOR_FORMAT_I1,
                     stride,
                     receipt.buf,
                     bufSize);

    receipt.bitmap = receipt.draw_buf.data + paletteSize;

    receipt.canvas = lv_canvas_create(NULL);
    lv_canvas_set_draw_buf(receipt.canvas, &receipt.draw_buf);
    lv_canvas_fill_bg(receipt.canvas, lv_color_white(), LV_OPA_COVER);
    lv_obj_center(receipt.canvas);
    storage = getStorage();
}

Receipt* createReceipt(void) {
    OOP_CALL_CTOR(Receipt, &receipt);
    return &receipt;
}