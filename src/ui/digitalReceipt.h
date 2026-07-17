#ifndef DIGITAL_RECEIPT_H
#define DIGITAL_RECEIPT_H

#include "mylvgl.h"

typedef struct {
    const char* dateTitle;
    const char* date;

    const char* timeTitle;
    const char* time;

    const char* amount;
    const char* currency;

    const char* statusTitle;
    const char* statusDescription;

    const void* statusIcon;
} ReceiptHeader;

typedef struct {
    const void* icon;

    const char* title;

    const char* value;

    lv_color_t valueColor;
} ReceiptDetail;

#define RECEIPT_MAX_DETAILS 4

typedef struct {
    ReceiptDetail details[RECEIPT_MAX_DETAILS];

    uint8_t count;
} ReceiptDetails;

typedef struct {
    const char*   text;
    const void*   icon;
    lv_event_cb_t callback;
    bool          primary;
} ReceiptButton;

typedef struct {
    ReceiptButton left;

    ReceiptButton right;
} ReceiptButtons;

typedef struct {
    lv_obj_t* root;

    /* Header */
    lv_obj_t* header;
    lv_obj_t* date;
    lv_obj_t* time;
    lv_obj_t* amount;
    lv_obj_t* statusTitle;
    lv_obj_t* statusDescription;
    lv_obj_t* statusIcon;

    /* Details */
    lv_obj_t* detailRow[RECEIPT_MAX_DETAILS];
    lv_obj_t* detailLine[RECEIPT_MAX_DETAILS];
    lv_obj_t* detailTitle[RECEIPT_MAX_DETAILS];
    lv_obj_t* detailValue[RECEIPT_MAX_DETAILS];
    lv_obj_t* detailIcon[RECEIPT_MAX_DETAILS];

    /* Buttons */
    lv_obj_t* leftButton;
    lv_obj_t* rightButton;
    lv_obj_t* btnPrimary;
    lv_obj_t* btnSecondary;
} ReceiptPage;

lv_obj_t* ui_receipt_create(ReceiptPage* page, const ReceiptHeader* header,
                            const ReceiptDetails* details,
                            const ReceiptButtons* buttons);

void ui_receipt_update(ReceiptPage* page, const ReceiptHeader* header,
                       const ReceiptDetails* details,
                       const ReceiptButtons* buttons);
void ui_receipt_hide(ReceiptPage* page);

#endif