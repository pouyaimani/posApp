#include "digitalReceipt.h"
#include "font/myFont.h"
#include "assets.h"
#include "display/display.h"
#include "error.h"

#define HEADER_HEIGHT             60
#define DETAIL_HEIGHT             100
#define DETAIL_ROW_TITLE_MAX_SIZE 120
#define BUTTON_HEIGHT             40
#define HEADER_DT_WIDTH           120

#define HEADER_TEXT_WIDTH 106

typedef struct {
    int32_t leftWidth;
    int32_t rightWidth;
} DetailRowLayout;

static lv_obj_t* create_header(ReceiptPage* page, const ReceiptHeader* cfg) {
    /*----------------------------------------------------------
     * Root
     *---------------------------------------------------------*/
    lv_obj_t* root = lv_obj_create(page->root);
    lv_obj_remove_style_all(root);

    lv_obj_set_width(root, LV_PCT(100));
    lv_obj_set_height(root, HEADER_HEIGHT);

    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_left(root, 2, 0);
    lv_obj_set_style_pad_right(root, 2, 0);
    lv_obj_set_style_pad_top(root, 1, 0);
    lv_obj_set_style_pad_bottom(root, 1, 0);

    /*----------------------------------------------------------
     * Left section (Date / Time)
     *---------------------------------------------------------*/
    lv_obj_t* left = lv_obj_create(root);
    lv_obj_remove_style_all(left);

    lv_obj_set_size(left, HEADER_DT_WIDTH, LV_PCT(100));

    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    /* Date row */
    {
        lv_obj_t* row = lv_obj_create(left);
        lv_obj_remove_style_all(row);

        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);

        /* Icon (left) */
        lv_obj_t* icon = lv_image_create(row);
        lv_image_set_src(icon, ICON_CALENDAR);

        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

        /* Date (right of icon) */
        lv_obj_t* label = lv_label_create(row);

        lv_label_set_text(label, cfg->date);
        lv_obj_set_style_text_font(label, &FONT_16, 0);

        lv_obj_align_to(label, icon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

        page->date = label;
    }

    /* Separator */
    {
        lv_obj_t* line = lv_obj_create(left);
        lv_obj_remove_style_all(line);

        lv_obj_set_width(line, LV_PCT(100));
        lv_obj_set_height(line, 1);

        lv_obj_set_style_bg_color(line, lv_palette_lighten(LV_PALETTE_GREY, 1),
                                  0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    }

    /* Time row */
    {
        lv_obj_t* row = lv_obj_create(left);
        lv_obj_remove_style_all(row);

        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);

        lv_obj_t* icon = lv_image_create(row);
        lv_image_set_src(icon, ICON_CLOCK);

        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* label = lv_label_create(row);

        lv_label_set_text(label, cfg->time);

        lv_obj_set_style_text_font(label, &FONT_16, 0);

        lv_obj_align_to(label, icon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
        page->time = label;
    }

    /*----------------------------------------------------------
     * Vertical divider
     *---------------------------------------------------------*/
    {
        lv_obj_t* divider = lv_obj_create(root);
        lv_obj_remove_style_all(divider);

        lv_obj_set_width(divider, 1);
        lv_obj_set_height(divider, 88);

        lv_obj_set_style_bg_color(divider,
                                  lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    }

    /*----------------------------------------------------------
     * Right section
     *---------------------------------------------------------*/
    lv_obj_t* right = lv_obj_create(root);
    lv_obj_remove_style_all(right);

    lv_obj_set_size(right, 170, LV_PCT(100));

    /*----------------------------------------------------------
     * Success icon
     *---------------------------------------------------------*/
    lv_obj_t* icon = lv_image_create(right);
    lv_image_set_src(icon, cfg->statusIcon);

    lv_obj_align(icon, LV_ALIGN_TOP_RIGHT, 4, 0);

    page->statusIcon = icon;

    /*----------------------------------------------------------
     * Amount
     *---------------------------------------------------------*/
    lv_obj_t* amount = lv_label_create(right);

    lv_label_set_text_fmt(amount, "%s %s", cfg->amount, cfg->currency);

    lv_obj_set_style_text_font(amount, &FONT_16, 0);

    lv_obj_set_style_text_color(amount, lv_palette_main(LV_PALETTE_GREEN), 0);

    lv_obj_align(amount, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_set_width(amount, HEADER_TEXT_WIDTH);

    lv_label_set_long_mode(amount, LV_LABEL_LONG_SCROLL_CIRCULAR);

    page->amount = amount;

    /*----------------------------------------------------------
     * Status title
     *---------------------------------------------------------*/
    lv_obj_t* title = lv_label_create(right);

    lv_label_set_text(title, cfg->statusTitle);

    lv_obj_set_style_text_font(title, &FONT_16, 0);

    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_GREEN), 0);
    // lv_obj_align(title, amount, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(title, HEADER_TEXT_WIDTH);

    lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);

    page->statusTitle = title;

    return root;
}

static DetailRowLayout
determine_detail_row_layout(lv_obj_t* row, lv_obj_t* title, lv_obj_t* icon) {
    DetailRowLayout layout = {0};

    lv_obj_update_layout(row);

    int32_t rowWidth   = lv_obj_get_content_width(row);
    int32_t iconWidth  = lv_obj_get_width(icon);
    int32_t titleWidth = lv_obj_get_width(title);

    /*
     * 4 px left padding
     * 4 px right padding
     * 8 px between title and icon
     */
    layout.rightWidth = iconWidth + titleWidth + 8;

    layout.leftWidth = rowWidth - layout.rightWidth - 8;

    if (layout.leftWidth < 20) {
        layout.leftWidth = 20;
    }

    return layout;
}

static lv_obj_t* create_detail_row(lv_obj_t* parent, lv_obj_t** title,
                                   lv_obj_t** value, lv_obj_t** icon,
                                   const ReceiptDetail* detail) {
    lv_obj_t* row = lv_obj_create(parent);

    lv_obj_remove_style_all(row);

    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 24);

    lv_obj_set_style_pad_left(row, 4, 0);
    lv_obj_set_style_pad_right(row, 4, 0);

    /*
     * Icon
     */
    *icon = lv_image_create(row);
    lv_image_set_src(*icon, detail->icon);

    lv_obj_align(*icon, LV_ALIGN_RIGHT_MID, 0, 0);

    /*
     * Title
     */
    *title = lv_label_create(row);

    lv_label_set_text(*title, detail->title);

    lv_obj_set_style_text_font(*title, &FONT_16, 0);

    lv_obj_align_to(*title, *icon, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    lv_obj_set_width(*title, DETAIL_ROW_TITLE_MAX_SIZE);

    lv_label_set_long_mode(*title, LV_LABEL_LONG_SCROLL);

    /*
     * Determine sizes.
     */
    DetailRowLayout layout = determine_detail_row_layout(row, *title, *icon);

    /*
     * Value
     */
    *value = lv_label_create(row);

    lv_obj_set_width(*value, layout.leftWidth);

    lv_label_set_long_mode(*value, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_label_set_text(*value, detail->value);

    lv_obj_set_style_text_font(*value, &FONT_16, 0);

    lv_obj_set_style_text_color(*value, detail->valueColor, 0);

    lv_obj_align(*value, LV_ALIGN_LEFT_MID, 0, 0);
    return row;
}

static lv_obj_t* create_details(ReceiptPage* page, const ReceiptDetails* cfg) {
    lv_obj_t* card = lv_obj_create(page->root);

    lv_obj_remove_style_all(card);

    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, DETAIL_HEIGHT);

    // lv_obj_set_style_radius(card, 14, 0);

    lv_obj_set_style_bg_color(card, lv_color_white(), 0);

    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    // lv_obj_set_style_pad_top(card, 1, 0);
    // lv_obj_set_style_pad_bottom(card, 1, 0);

    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (uint32_t i = 0; i < cfg->count; i++) {
        page->detailRow[i] = create_detail_row(
            card, &page->detailTitle[i], &page->detailValue[i],
            &page->detailIcon[i], &cfg->details[i]);

        if (i != (cfg->count - 1)) {
            lv_obj_t* line = lv_line_create(card);

            static lv_point_precise_t pts[] = {
                {0, 0},
                {280, 0},
            };

            lv_line_set_points(line, pts, 2);

            lv_obj_set_width(line, LV_PCT(94));

            lv_obj_set_style_line_color(
                line, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);

            lv_obj_set_style_line_width(line, 1, 0);

            lv_obj_set_style_line_dash_width(line, 4, 0);
            lv_obj_set_style_line_dash_gap(line, 4, 0);
            page->detailLine[i] = line;
        }
    }

    return card;
}

static lv_obj_t* create_buttons(ReceiptPage* page, const ReceiptButtons* cfg) {
    lv_obj_t* root = lv_obj_create(page->root);
    lv_obj_remove_style_all(root);

    lv_obj_set_width(root, LV_PCT(100));
    lv_obj_set_height(root, BUTTON_HEIGHT);

    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_column(root, 4, 0);
    lv_obj_set_style_pad_all(root, 0, 0);

    const ReceiptButton* buttons[2] = {&cfg->left, &cfg->right};

    for (uint32_t i = 0; i < 2; i++) {
        const ReceiptButton* btnCfg = buttons[i];

        lv_obj_t* btn = lv_button_create(root);

        lv_obj_set_size(btn, LV_PCT(48), LV_SIZE_CONTENT);

        lv_obj_set_style_radius(btn, 8, 0);

        if (btnCfg->primary) {
            lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN),
                                      0);

            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn,
                                          lv_palette_main(LV_PALETTE_GREEN), 0);

            lv_obj_set_style_text_color(btn, lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_white(), 0);

            lv_obj_set_style_border_width(btn, 1, 0);

            lv_obj_set_style_border_color(
                btn, lv_palette_main(LV_PALETTE_DEEP_PURPLE), 0);

            lv_obj_set_style_text_color(
                btn, lv_palette_main(LV_PALETTE_DEEP_PURPLE), 0);
        }

        if (btnCfg->callback) {
            lv_obj_add_event_cb(btn, btnCfg->callback, LV_EVENT_CLICKED, NULL);
        }

        /*------------------------------------------------------
         * Content
         *-----------------------------------------------------*/
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW_REVERSE);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_pad_column(btn, 4, 0);

        if (btnCfg->icon) {
            lv_obj_t* img = lv_image_create(btn);

            lv_image_set_src(img, btnCfg->icon);

            if (btnCfg->primary) {
                lv_obj_set_style_image_recolor(img, lv_color_white(), 0);

                lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
            }
        }
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, btnCfg->text);

        lv_obj_set_style_text_font(label, &FONT_16, 0);

        if (btnCfg->primary) {
            page->btnPrimary = label;
        } else {
            page->btnSecondary = label;
        }
    }

    return root;
}

lv_obj_t* ui_receipt_create(ReceiptPage* page, const ReceiptHeader* header,
                            const ReceiptDetails* details,
                            const ReceiptButtons* buttons) {
    RETURN_VALUE_IF_NULL(header, ;, NULL);
    RETURN_VALUE_IF_NULL(details, ;, NULL);
    RETURN_VALUE_IF_NULL(buttons, ;, NULL);

    /*----------------------------------------------------------
     * Root
     *---------------------------------------------------------*/
    lv_obj_t* root = lv_obj_create(disp()->screen);

    lv_obj_remove_style_all(root);

    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));

    lv_obj_set_style_bg_color(root, lv_palette_lighten(LV_PALETTE_GREY, 4), 0);

    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    lv_obj_set_style_pad_top(root, 2, 0);
    lv_obj_set_style_pad_right(root, 6, 0);
    lv_obj_set_style_pad_left(root, 6, 0);

    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);

    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_row(root, 4, 0);

    page->root = root;

    /*----------------------------------------------------------
     * Header
     *---------------------------------------------------------*/
    create_header(page, header);

    /*----------------------------------------------------------
     * Details
     *---------------------------------------------------------*/
    create_details(page, details);

    /*----------------------------------------------------------
     * Spacer
     *---------------------------------------------------------*/
    // lv_obj_t* spacer = lv_obj_create(root);

    // lv_obj_remove_style_all(spacer);

    // lv_obj_set_flex_grow(spacer, 1);

    /*----------------------------------------------------------
     * Buttons
     *---------------------------------------------------------*/
    create_buttons(page, buttons);

    return root;
}

void ui_receipt_update(ReceiptPage* page, const ReceiptHeader* header,
                       const ReceiptDetails* details,
                       const ReceiptButtons* buttons) {
    RETURN_IF_NULL(page, ;);
    RETURN_IF_NULL(header, ;);
    RETURN_IF_NULL(details, ;);
    RETURN_IF_NULL(buttons, ;);

    // // Header
    // lv_label_set_text(page->date, header->date);
    // lv_label_set_text(page->time, header->time);
    // lv_label_set_text_fmt(page->amount, "%s %s", header->amount,
    //                       header->currency);
    // lv_label_set_text(page->statusTitle, header->statusTitle);
    // lv_img_set_src(page->statusIcon, header->statusIcon);

    // // Details
    // for (uint32_t i = 0; i < details->count; i++) {

    //     lv_label_set_text(page->detailTitle[i], details->details[i].title);
    //     lv_label_set_text(page->detailValue[i], details->details[i].value);
    //     lv_label_set_text(page->detailIcon[i], details->details[i].icon);
    // }

    // // Buttons
    // lv_label_set_text(page->btnPrimary, buttons->right.text);
    // lv_label_set_text(page->btnSecondary, buttons->left.text);
    // // Details

    for (uint32_t i = 0; i < details->count; i++) {
        /*
         * Determine sizes.
         */
        LV_SHOW(page->detailRow[i]);
        LV_SHOW(page->detailLine[i - 1]);
        lv_obj_update_layout(page->detailRow[i]);
        DetailRowLayout layout = determine_detail_row_layout(
            page->detailRow[i], page->detailTitle[i], page->detailIcon[i]);
        lv_obj_set_width(page->detailValue[i], layout.leftWidth);
    }

    for (uint32_t i = details->count; i < RECEIPT_MAX_DETAILS; i++) {
        LV_HIDE(page->detailRow[i]);
        LV_HIDE(page->detailLine[i - 1]);
    }
}

void ui_receipt_hide(ReceiptPage* page) { LV_HIDE(page->root); }