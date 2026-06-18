#pragma once

#include "lvgl.h"

typedef enum { SWIPE_DOWN, SWIPE_UP } SwipeDirection;

#define SWIPE_MAX_ROWS 8

typedef struct {

    lv_obj_t* root;

    lv_obj_t* rows[SWIPE_MAX_ROWS];

    lv_timer_t* timer;

    uint8_t count;

    uint8_t index;

    uint8_t phase;

} SwipeHint;

void swipeHintCreate(SwipeHint*, lv_obj_t*, SwipeDirection, uint8_t count);

void swipeHintShow(SwipeHint*);

void swipeHintHide(SwipeHint*);

void swipeHintAlign(SwipeHint*, lv_align_t, int32_t, int32_t);