#ifndef DIGITAL_RECEIPT_H
#define DIGITAL_RECEIPT_H

#include "mylvgl.h"

#define RECEIPT_MAX_ROWS 8

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

#define RECEIPT_MAX_DETAILS 8

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

lv_obj_t* ui_receipt_create(const ReceiptHeader*  header,
                            const ReceiptDetails* details,
                            const ReceiptButtons* buttons);

#endif