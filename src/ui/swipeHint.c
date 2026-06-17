/* swipeHint.c */

#include "swipeHint.h"
#include <string.h>
#include "mylvgl.h"
#include "myColor.h"

#define CHEVRON_W          28
#define CHEVRON_H          12

#define STEP_DELAY_MS      120
#define HOLD_TIME_MS       350
#define PAUSE_TIME_MS      700

static void hideAll(SwipeHint *s)
{
    for(int i = 0; i < s->count; i++)
    {
        lv_obj_add_flag(
            s->rows[i],
            LV_OBJ_FLAG_HIDDEN);
    }
}

static void showRow(
    SwipeHint *s,
    uint8_t idx)
{
    if(idx >= s->count)
        return;

    lv_obj_clear_flag(
        s->rows[idx],
        LV_OBJ_FLAG_HIDDEN);
}

static void timerCb(
    lv_timer_t *timer)
{
    SwipeHint *s =
        lv_timer_get_user_data(timer);

    if(s == NULL)
        return;

    switch(s->phase)
    {
    /* ---------- SHOW ---------- */

    case 0:

        if(s->index < s->count)
        {
            lv_obj_clear_flag(
                s->rows[s->index],
                LV_OBJ_FLAG_HIDDEN);

            s->index++;
        }

        if(s->index >= s->count)
        {
            s->phase = 1;

            s->index = 0;

            lv_timer_set_period(
                timer,
                STEP_DELAY_MS);
        }

        break;

    /* ---------- HIDE ---------- */

    case 1:

        if(s->index < s->count)
        {
            lv_obj_add_flag(
                s->rows[s->index],
                LV_OBJ_FLAG_HIDDEN);

            s->index++;
        }

        if(s->index >= s->count)
        {
            s->phase = 2;

            lv_timer_set_period(
                timer,
                PAUSE_TIME_MS);
        }

        break;

    /* ---------- WAIT ---------- */

    case 2:

        s->phase = 0;

        s->index = 0;

        lv_timer_set_period(
            timer,
            STEP_DELAY_MS);

        break;
    }
}

static lv_obj_t *createChevron(
    lv_obj_t *parent)
{
    lv_obj_t *label =
        lv_label_create(parent);
    LV_SET_TEXT_FONT(label, lv_font_montserrat_16);
    LV_SET_TEXT_COLOR(label, MAIN_THEME_COLOR);
    LV_SET_TEXT(label,
        LV_SYMBOL_DOWN);

    return label;
}

void swipeHintCreate(
    SwipeHint *s,
    lv_obj_t *parent,
    SwipeDirection dir,
    uint8_t count)
{
    memset(
        s,
        0,
        sizeof(*s));

    if(count > SWIPE_MAX_ROWS)
        count = SWIPE_MAX_ROWS;

    s->count =
        count;

    s->root =
        lv_obj_create(parent);

    lv_obj_remove_style_all(
        s->root);

    LV_SET_SIZE(s->root, 60, count * 20);

    for(int i = 0; i < count; i++)
    {
        s->rows[i] =
            createChevron(
                s->root);

        int pos =
            (dir == SWIPE_DOWN)
            ? i
            : (count - 1 - i);

        lv_obj_align(
            s->rows[i],
            LV_ALIGN_TOP_MID,
            0,
            pos * 16);

        LV_HIDE(
            s->rows[i]);
    }
}

void swipeHintShow(
    SwipeHint *s)
{
    hideAll(s);

    s->phase = 0;
    s->index = 0;

    LV_SHOW(
        s->root);

    if(s->timer)
    {
        lv_timer_del(
            s->timer);
    }

    s->timer =
        lv_timer_create(
            timerCb,
            STEP_DELAY_MS,
            s);
}

void swipeHintHide(
    SwipeHint *s)
{
    if(s->timer)
    {
        lv_timer_del(
            s->timer);

        s->timer =
            NULL;
    }

    hideAll(s);

    LV_HIDE(
        s->root);
}

void swipeHintAlign(
    SwipeHint *s,
    lv_align_t align,
    int32_t x,
    int32_t y)
{
    lv_obj_align(
        s->root,
        align,
        x,
        y);
}