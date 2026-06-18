#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H

#include "mylvgl.h"

#define DOT_COUNT  12
#define DOT_SIZE   8
#define DOT_RADIUS 30

#define CIRCLE_SIZE 30

#define RIPPLE_START 60
#define RIPPLE_END   140

#define RIPPLE_TIME  600
#define RIPPLE_DELAY 120

#define WAIT_PERIOD 100

typedef enum {
    STATUS_INDICATOR_HIDDEN = 0,
    STATUS_INDICATOR_WAITING,
    STATUS_INDICATOR_SUCCESS,
    STATUS_INDICATOR_ERROR,
    STATUS_INDICATOR_WARNING
} StatusIndicatorState;

typedef struct {
    lv_obj_t* root;

    lv_obj_t* circle;
    lv_obj_t* icon;

    lv_obj_t* ripple1;
    lv_obj_t* ripple2;

    uint32_t color;

    lv_obj_t* dots[DOT_COUNT];

    lv_timer_t* waitTimer;

    uint8_t waitIndex;

    StatusIndicatorState state;

} StatusIndicator;

void statusIndicatorCreate(StatusIndicator* si, lv_obj_t* parent);

void statusIndicatorShow(StatusIndicator* si, StatusIndicatorState state);

void statusIndicatorHide(StatusIndicator* si);

void statusIndicatorAlign(StatusIndicator* si, lv_align_t align, int32_t x,
                          int32_t y);

#endif