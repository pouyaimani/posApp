#include "receipt.h"
#include "lvgl.h"
#include "sys/sys.h"
#include "logger.h"
#include "font/myFont.h"
#include "display/display.h"
#include "printer/printer.h"
#include "settings/settings.h"
#include "assets.h"
#include "utility/utility.h"
#include "phrases/phrases.h"

#define PRINTER_WIDTH_PIX 384
#define MAX_HEIGHT        PRINTER_WIDTH_PIX // dynamic safe max
#define SHAPED_MAX        128

/* =========================
   Internal Helpers
   ========================= */

static int safe_shape(const char* in, char* out, size_t max) {
    if (!in || !out) {
        return ERR_BAD_PARAMETER;
    }
    size_t len = strlen(in);
    if (len >= max) {
        LOG_ERROR("Text too long for shaping buffer");
        return ERR_NOK;
    }
    lv_text_ap_proc(in, out);
    return ERR_OK;
}

static Result_t flushReceipt(Receipt* rec) {
    Result_t res = {.err = ERR_DSC_PRINTER};
    RETURN_VALUE_IF_NULL(rec, res.err = ERR_DSC_INVALID_ARG;, res);
    // Send current buffer to printer
    res.detail.printer = printer()->print(rec->bitmap, rec->width, rec->height);

    // TODO: handle printer error
    RETURN_VALUE_IF_NOT(
        res.detail.printer, PRNT_ERR_OK, res.err = ERR_DSC_PRINTER;, res);

    // Clear canvas
    lv_canvas_fill_bg(rec->canvas, lv_color_white(), LV_OPA_COVER);

    // Reset cursor
    rec->height = 0;
    return res;
}

static bool flushIfNeeded(Receipt* r, uint16_t next_h) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    if (next_h > r->maxHeight) {
        LOG_TRACE("Receipt: height of object is greater than maximum. max "
                  "height = %d, object height = %d",
                  r->maxHeight, next_h);
        return false;
    }
    if (r->height + next_h > r->maxHeight) {
        flushReceipt(r);
    }
    return true;
}

static void draw_text_line(lv_layer_t* layer, int x, int y, int w, int height,
                           const char* txt, lv_text_align_t align,
                           const lv_font_t* font) {
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color    = lv_color_black();
    dsc.flag     = 0;
    dsc.bidi_dir = LV_BASE_DIR_AUTO;
    dsc.font     = font;
    dsc.align    = align;
    dsc.text     = txt;

    lv_area_t coords = {
        .x1 = x, .y1 = y, .x2 = x + w - 1, .y2 = y + height - 1};

    lv_draw_label(layer, &dsc, &coords);
}

static uint16_t measure_text_height(const char* txt, int width,
                                    const lv_font_t* font) {
    lv_point_t size;
    lv_text_get_size(&size, txt, font, 0, 0, width, LV_TEXT_FLAG_NONE);
    return size.y;
}

static int8_t addText(Receipt* r, int count, const RecColumn_t* cols) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);

    int totalWeight = 0;
    if (count > MAX_CULOMN_CNT) {
        LOG_ERROR("Too many columns");
        count = MAX_CULOMN_CNT;
    }
    if (count == 0) {
        return ERR_NOK;
    }
    if (!cols) {
        return ERR_NOK;
    }
    for (int i = 0; i < count; i++)
        totalWeight += cols[i].weight;
    RETURN_VALUE_IF_NOT((totalWeight <= 0), false, ;, ERR_NOK);
    uint16_t height    = 0;
    int      remaining = PRINTER_WIDTH_PIX;
    for (int i = 0; i < count; i++) {
        int w = (i == count - 1)
                    ? remaining
                    : (PRINTER_WIDTH_PIX * cols[i].weight) / totalWeight;
        remaining -= w;
        RETURN_VALUE_IF_NOT(safe_shape(cols[i].src, r->shaped[i], SHAPED_MAX),
                            ERR_OK,
                            ;, ERR_NOK);
        uint16_t h = measure_text_height(r->shaped[i], w, &FONT_16);
        if (h > height)
            height = h;
    }

    RETURN_VALUE_IF_NOT(flushIfNeeded(r, height), true, ;, ERR_NOK);

    int        x = 0;
    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);
    remaining = PRINTER_WIDTH_PIX;
    for (int i = 0; i < count; i++) {
        int w = (i == count - 1)
                    ? remaining
                    : (PRINTER_WIDTH_PIX * cols[i].weight) / totalWeight;

        remaining -= w;
        draw_text_line(&layer, x, r->height, w, height, r->shaped[i],
                       cols[i].align, &FONT_16);

        x += w;
    }
    lv_canvas_finish_layer(r->canvas, &layer);
    r->height += height;
    return ERR_OK;
}

static int8_t addTable(Receipt* r, int count, const RecColumn_t* cols) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);
    if (count > MAX_CULOMN_CNT) {
        LOG_ERROR("Too many columns");
        count = MAX_CULOMN_CNT;
    }
    RETURN_VALUE_IF_NOT(count == 0, false, ;, ERR_NOK);
    RETURN_VALUE_IF_NULL(cols, ;, ERR_NOK);
    int      totalWeight = count; // equal width columns
    int      x           = 0;
    uint16_t max_h       = 0;

    // Calculate row height
    for (int i = 0; i < count; i++) {
        uint32_t w = PRINTER_WIDTH_PIX / totalWeight;
        RETURN_VALUE_IF_NOT(safe_shape(cols[i].src, r->shaped[i], SHAPED_MAX),
                            ERR_OK,
                            ;, ERR_NOK);
        uint16_t h = measure_text_height(cols[i].src, w - 4, &FONT_16);
        if (h > max_h)
            max_h = h;
    }

    // Add some padding
    // TODO: add pading based on font
    max_h += 6;

    // Flush if needed
    RETURN_VALUE_IF_NOT(flushIfNeeded(r, max_h), true, ;, ERR_NOK);

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
    for (int i = 0; i < count; i++) {
        int w = PRINTER_WIDTH_PIX / totalWeight;

        // Draw vertical line (left border + separators)
        line.p1.x = x;
        line.p1.y = row_y1;
        line.p2.x = x;
        line.p2.y = row_y2;
        lv_draw_line(&layer, &line);

        // Draw text (with padding)
        draw_text_line(&layer, x + 2, r->height + 2, w - 4, max_h, cols[i].src,
                       cols[i].align, &FONT_16);

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

    return ERR_OK;
}

/* -------- IMAGE -------- */
static int8_t addImage(Receipt* r, int count, const RecColumn_t* cols) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(cols, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT((count <= 0), false, ;, ERR_BAD_PARAMETER);

    if (count > MAX_CULOMN_CNT) {
        LOG_ERROR("Too many columns");
        count = MAX_CULOMN_CNT;
    }

    int totalWeight = 0;

    for (int i = 0; i < count; i++) {
        totalWeight += cols[i].weight;
    }
    RETURN_VALUE_IF_NOT((totalWeight <= 0), false, ;, ERR_NOK);

    /*--------------------------------------------------
     * Determine row height
     *--------------------------------------------------*/

    uint16_t maxHeight = 0;

    for (int i = 0; i < count; i++) {

        const lv_image_dsc_t* img = (const lv_image_dsc_t*)cols[i].src;

        if (!img) {
            continue;
        }
        LOG_TRACE("column %d image height = %d", i, img->header.h);
        if (img->header.h > maxHeight) {
            maxHeight = img->header.h;
        }
    }

    RETURN_VALUE_IF_NOT((maxHeight == 0), false, ;, ERR_NOK);

    RETURN_VALUE_IF_NOT(flushIfNeeded(r, maxHeight), true, ;, ERR_NOK);

    /*--------------------------------------------------
     * Draw images
     *--------------------------------------------------*/

    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);

    int x         = 0;
    int remaining = PRINTER_WIDTH_PIX;

    for (int i = 0; i < count; i++) {

        int w = (i == count - 1)
                    ? remaining
                    : (PRINTER_WIDTH_PIX * cols[i].weight) / totalWeight;

        remaining -= w;

        const lv_image_dsc_t* img = (const lv_image_dsc_t*)cols[i].src;

        if (img) {

            int drawX;

            switch (cols[i].align) {

            case LV_ALIGN_LEFT_MID:
                drawX = x;
                break;

            case LV_ALIGN_CENTER:
                drawX = x + (w - img->header.w) / 2;
                break;

            case LV_ALIGN_RIGHT_MID:
                drawX = x + w - img->header.w;
                break;

            default:
                drawX = x;
                break;
            }

            int drawY = r->height + (maxHeight - img->header.h) / 2;

            lv_area_t coords = {
                .x1 = drawX,
                .y1 = drawY,
                .x2 = drawX + img->header.w - 1,
                .y2 = drawY + img->header.h - 1,
            };

            lv_draw_image_dsc_t img_dsc;
            lv_draw_image_dsc_init(&img_dsc);
            img_dsc.src = img;

            lv_draw_image(&layer, &img_dsc, &coords);
        }

        x += w;
    }

    lv_canvas_finish_layer(r->canvas, &layer);

    r->height += maxHeight;

    return ERR_OK;
}

/* -------- SPACE -------- */
static int8_t addSpace(Receipt* r, uint16_t h) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NOT(flushIfNeeded(r, h), true, ;, ERR_NOK);
    r->height += h;
    return ERR_OK;
}

static int8_t addHighlightedText(Receipt* r, const char* text,
                                 const lv_font_t* font, lv_text_align_t align) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);
    if (!font)
        font = &FONT_16;

    // Measure text ---
    uint16_t text_h = measure_text_height(text, PRINTER_WIDTH_PIX, font);

    // TODO: add pading based on font
    uint16_t padding_top    = 4;
    uint16_t padding_bottom = 4;
    uint16_t height         = text_h + padding_top + padding_bottom;

    // Flush if needed ---
    RETURN_VALUE_IF_NOT(flushIfNeeded(r, height), true, ;, ERR_NOK);

    // Start drawing ---
    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);

    // Draw BLACK background ---
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_black();
    rect_dsc.bg_opa   = LV_OPA_COVER;

    lv_area_t bg = {.x1 = 0,
                    .y1 = r->height,
                    .x2 = PRINTER_WIDTH_PIX - 1,
                    .y2 = r->height + height - 1};

    lv_draw_rect(&layer, &rect_dsc, &bg);

    // Draw WHITE text ---
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_white();
    label_dsc.font  = font;
    label_dsc.align = align;
    label_dsc.flag |= LV_TEXT_FLAG_EXPAND;
    label_dsc.bidi_dir = LV_BASE_DIR_AUTO;
    if (safe_shape(text, r->shaped[0], SHAPED_MAX) != ERR_OK) {
        lv_canvas_finish_layer(r->canvas, &layer);
        return ERR_NOK;
    }
    label_dsc.text = r->shaped[0];

    lv_area_t txt_area = {.x1 = 0,
                          .y1 = r->height + padding_top,
                          .x2 = PRINTER_WIDTH_PIX - 1,
                          .y2 = r->height + padding_top + text_h - 1};

    lv_draw_label(&layer, &label_dsc, &txt_area);

    // Finish ---
    lv_canvas_finish_layer(r->canvas, &layer);

    r->height += height;

    return ERR_OK;
}

static int8_t addAmount(Receipt* r, const char* amount) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);
    if (!amount) {
        return ERR_BAD_PARAMETER;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), " %s: %s %s ", phraseGetDef(PHRASE_AMOUNT),
             amount, phraseGetDef(PHRASE_RIAL));

    return addHighlightedText(r, buf, &FONT_16, LV_TEXT_ALIGN_CENTER);
}

/* -------- HEADER -------- */
static int8_t addHeader(Receipt* r, uint32_t date, uint32_t time) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);
    TerminalSettings* t = &settings()->terminal;

    RecColumn_t row1[] = {
        {"TODO", LV_TEXT_ALIGN_LEFT, 1},
        {phraseGetDef(PHRASE_PSP_GREEN_PAYMENT), LV_TEXT_ALIGN_RIGHT, 1}};
    if (addText(r, 2, row1) != ERR_OK) {
        return ERR_NOK;
    }
    DEFINE_STRING(dt, 64);
    dateTimeToStr(date, time, dt, sizeof(dt));

    DEFINE_STRING(buf, 64);
    snprintf(buf, sizeof(buf), "%s - %s", dt, "TODO");

    RecColumn_t row2[] = {{buf, LV_TEXT_ALIGN_LEFT, 1}};

    return addHighlightedText(r, buf, &FONT_16, row2);
}

static int8_t addTextWithBorder(Receipt* r, int count,
                                const RecColumn_t* cols) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(cols, ;, ERR_BAD_PARAMETER);

    if (count <= 0) {
        return ERR_BAD_PARAMETER;
    }

    if (count > MAX_CULOMN_CNT) {
        LOG_ERROR("Too many columns");
        count = MAX_CULOMN_CNT;
    }

    int totalWeight = 0;

    for (int i = 0; i < count; i++) {
        totalWeight += cols[i].weight;
    }

    RETURN_VALUE_IF_NOT((totalWeight <= 0), false, ;, ERR_NOK);

    uint16_t max_h = 0;

    for (int i = 0; i < count; i++) {

        uint32_t w = (PRINTER_WIDTH_PIX * cols[i].weight) / totalWeight;
        RETURN_VALUE_IF_NOT(safe_shape(cols[i].src, r->shaped[i], SHAPED_MAX),
                            ERR_OK,
                            ;, ERR_NOK);

        uint16_t h = measure_text_height(r->shaped[i], w - 6, &FONT_16);

        if (h > max_h) {
            max_h = h;
        }
    }

    max_h += 6;

    RETURN_VALUE_IF_NOT(flushIfNeeded(r, max_h), true, ;, ERR_NOK);

    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);

    line.color = lv_color_black();
    line.width = 1;

    int row_y1 = r->height;
    int row_y2 = r->height + max_h;

    // Top border
    line.p1.x = 0;
    line.p1.y = row_y1;
    line.p2.x = PRINTER_WIDTH_PIX - 1;
    line.p2.y = row_y1;
    lv_draw_line(&layer, &line);

    // Bottom border
    line.p1.y = row_y2;
    line.p2.y = row_y2;
    lv_draw_line(&layer, &line);

    int x         = 0;
    int remaining = PRINTER_WIDTH_PIX;

    for (int i = 0; i < count; i++) {

        int w = (i == count - 1)
                    ? remaining
                    : (PRINTER_WIDTH_PIX * cols[i].weight) / totalWeight;

        remaining -= w;

        // Vertical separator
        line.p1.x = x;
        line.p1.y = row_y1;
        line.p2.x = x;
        line.p2.y = row_y2;
        lv_draw_line(&layer, &line);

        // Text
        draw_text_line(&layer, x + 3, r->height + 3, w - 6, max_h - 6,
                       r->shaped[i], cols[i].align, &FONT_16);

        x += w;
    }

    // Right border
    line.p1.x = PRINTER_WIDTH_PIX - 1;
    line.p1.y = row_y1;
    line.p2.x = PRINTER_WIDTH_PIX - 1;
    line.p2.y = row_y2;
    lv_draw_line(&layer, &line);

    lv_canvas_finish_layer(r->canvas, &layer);

    r->height += max_h;

    return ERR_OK;
}

static int8_t addLineHorizontal(Receipt* r, uint16_t thickness,
                                uint16_t paddingTop, uint16_t paddingBottom) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);

    if (thickness == 0) {
        thickness = 1;
    }

    uint16_t total_h = paddingTop + thickness + paddingBottom;

    RETURN_VALUE_IF_NOT(flushIfNeeded(r, total_h), true, ;, ERR_NOK);

    lv_layer_t layer;
    lv_canvas_init_layer(r->canvas, &layer);

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);

    line.color = lv_color_black();
    line.width = thickness;

    int y = r->height + paddingTop;

    line.p1.x = 0;
    line.p1.y = y;

    line.p2.x = PRINTER_WIDTH_PIX - 1;
    line.p2.y = y;

    lv_draw_line(&layer, &line);

    lv_canvas_finish_layer(r->canvas, &layer);

    r->height += total_h;

    return ERR_OK;
}

/* -------- FOOTER -------- */
static int8_t addFooter(Receipt* r) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->buf, ;, ERR_BAD_PARAMETER);
    RETURN_VALUE_IF_NULL(r->canvas, ;, ERR_BAD_PARAMETER);

    RecColumn_t row1[] = {{ICON_BANK_REC, LV_ALIGN_LEFT_MID, 1},
                          {ICON_SHAPARAK, LV_ALIGN_RIGHT_MID, 1}};
    return addImage(r, 2, row1);
}

static void destroyReceipt(Receipt* r) {
    RETURN_VALUE_IF_NULL(r, ;, ERR_BAD_PARAMETER);
    if (r->canvas) {
        LV_DELETE(r->canvas);
        r->canvas = NULL;
    }
    if (r->buf) {
        MEM_FREE(r->buf);
        r->buf = NULL;
    }
}

OOP_CTOR(Receipt) {
    self->vtable.addText            = addText;
    self->vtable.addSpace           = addSpace;
    self->vtable.addTable           = addTable;
    self->vtable.addImage           = addImage;
    self->vtable.addTextWithBorder  = addTextWithBorder;
    self->vtable.addHeader          = addHeader;
    self->vtable.addFooter          = addFooter;
    self->vtable.addHighlightedText = addHighlightedText;
    self->vtable.destroy            = destroyReceipt;
    self->vtable.flush              = flushReceipt;
    self->vtable.addAmount          = addAmount;
    self->vtable.addLineHorizontal  = addLineHorizontal;

    self->maxHeight      = MAX_HEIGHT;
    self->height         = 0;
    self->width          = PRINTER_WIDTH_PIX;
    uint32_t stride      = (PRINTER_WIDTH_PIX + 7) / 8; // bytes per row
    uint32_t paletteSize = LV_COLOR_INDEXED_PALETTE_SIZE(LV_COLOR_FORMAT_I1) *
                           sizeof(lv_color32_t);

    uint32_t bufSize = stride * MAX_HEIGHT + LV_DRAW_BUF_ALIGN + paletteSize;
    self->buf        = MEM_ALLOC(bufSize);

    if (!self->buf) {
        LOG_ERROR("Receipt: error in allocating memory for buffer.");
        self->canvas = NULL;
        self->bitmap = NULL;
        return;
    }

    lv_draw_buf_init(&self->draw_buf, PRINTER_WIDTH_PIX, MAX_HEIGHT,
                     LV_COLOR_FORMAT_I1, stride, self->buf, bufSize);

    self->bitmap = self->draw_buf.data + paletteSize;

    self->canvas = lv_canvas_create(NULL);
    lv_canvas_set_draw_buf(self->canvas, &self->draw_buf);
    lv_canvas_fill_bg(self->canvas, lv_color_white(), LV_OPA_COVER);
    lv_obj_center(self->canvas);
}

int8_t createReceipt(Receipt* receipt) {
    OOP_CALL_CTOR(Receipt, receipt);
    RETURN_VALUE_IF_NULL(receipt->canvas, MEM_FREE(receipt->buf),
                         ERR_MEMORY_ALLOCATION);
    RETURN_VALUE_IF_NULL(receipt->buf, MEM_FREE(receipt->canvas),
                         ERR_MEMORY_ALLOCATION);
    return ERR_OK;
}