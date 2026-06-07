#include "statusIndicator.h"
#include "myColor.h"

static void rippleSizeCb(
    void *obj,
    int32_t v)
{
    lv_obj_set_size(obj, v, v);

    lv_obj_center(obj);
}

static void rippleOpaCb(
    void *obj,
    int32_t v)
{
    lv_obj_set_style_border_opa(
        obj,
        v,
        0);
}

static void setColor(
    StatusIndicator *si,
    uint32_t color)
{
    si->color = color;
    LV_SET_BG_COLOR(si->circle, color);
    LV_SET_TEXT_COLOR(si->icon, COLOR_WHITE);
    LV_SET_BORDER_COLOR(si->ripple1, color);
    LV_SET_BORDER_COLOR(si->ripple2, color);
    for(int i = 0; i < DOT_COUNT; i++) {
        LV_SET_BG_COLOR(si->dots[i], color);
    }
}

static void startRipple(
    lv_obj_t *obj,
    uint32_t delay)
{
    lv_anim_t a;

    lv_anim_init(&a);

    lv_anim_set_var(
        &a,
        obj);

    lv_anim_set_exec_cb(
        &a,
        rippleSizeCb);

    lv_anim_set_values(
        &a,
        RIPPLE_START,
        RIPPLE_END);

    lv_anim_set_time(
        &a,
        RIPPLE_TIME);

    lv_anim_set_delay(
        &a,
        delay);

    lv_anim_start(&a);

    lv_anim_init(&a);

    lv_anim_set_var(
        &a,
        obj);

    lv_anim_set_exec_cb(
        &a,
        rippleOpaCb);

    lv_anim_set_values(
        &a,
        LV_OPA_COVER,
        LV_OPA_TRANSP);

    lv_anim_set_time(
        &a,
        RIPPLE_TIME);

    lv_anim_set_delay(
        &a,
        delay);

    lv_anim_start(&a);
}

static void waitTimerCb(
    lv_timer_t *timer)
{
    StatusIndicator *si =
        lv_timer_get_user_data(timer);

    if(si == NULL)
        return;

    uint8_t head =
        si->waitIndex;

    for(int i = 0; i < DOT_COUNT; i++)
    {
        uint8_t dist =
            (i + DOT_COUNT - head)
            % DOT_COUNT;

        lv_opa_t opa;

        switch(dist)
        {
        case 0:
            opa = LV_OPA_100;
            break;

        case 1:
            opa = LV_OPA_60;
            break;

        case 2:
            opa = LV_OPA_30;
            break;

        default:
            opa = LV_OPA_10;
            break;
        }

        lv_obj_set_style_bg_opa(
            si->dots[i],
            opa,
            0);
    }

    si->waitIndex =
        (head + 1) % DOT_COUNT;
}

void statusIndicatorCreate(
    StatusIndicator *si,
    lv_obj_t *parent)
{
    memset(
        si,
        0,
        sizeof(*si));

    si->root =
        lv_obj_create(parent);

    lv_obj_remove_style_all(
        si->root);

    lv_obj_set_size(
        si->root,
        160,
        160);
    si->circle =
        lv_obj_create(si->root);

    lv_obj_set_style_bg_opa(
        si->circle,
        LV_OPA_COVER,
        0);

    lv_obj_set_size(
        si->circle,
        CIRCLE_SIZE,
        CIRCLE_SIZE);

    lv_obj_center(
        si->circle);

    lv_obj_set_style_radius(
        si->circle,
        LV_RADIUS_CIRCLE,
        0);
    LV_SET_BG_OPA(si->circle, LV_OPA_40);
    si->icon =
        lv_label_create(
            si->root);

    lv_obj_set_style_text_font(si->icon, LV_FONT_DEFAULT, LV_PART_MAIN);

    lv_obj_center(
        si->icon);

    si->ripple1 =
        lv_obj_create(
            si->root);

    si->ripple2 =
        lv_obj_create(
            si->root);
    lv_obj_remove_style_all(si->ripple1);
    lv_obj_remove_style_all(si->ripple2);

    lv_obj_set_size(si->ripple1, RIPPLE_START, RIPPLE_START);
    lv_obj_set_size(si->ripple2, RIPPLE_START, RIPPLE_START);

    lv_obj_center(si->ripple1);
    lv_obj_center(si->ripple2);

    lv_obj_set_style_radius(
        si->ripple1,
        LV_RADIUS_CIRCLE,
        0);

    lv_obj_set_style_radius(
        si->ripple2,
        LV_RADIUS_CIRCLE,
        0);

    lv_obj_set_style_bg_opa(
        si->ripple1,
        LV_OPA_TRANSP,
        0);

    lv_obj_set_style_bg_opa(
        si->ripple2,
        LV_OPA_TRANSP,
        0);

    lv_obj_set_style_border_width(
        si->ripple1,
        2,
        0);

    lv_obj_set_style_border_width(
        si->ripple2,
        2,
        0);
    for(int i = 0 ; i < DOT_COUNT ; i++) {
        si->dots[i] =
            lv_obj_create(
                si->root);

        lv_obj_remove_style_all(
            si->dots[i]);

        lv_obj_set_size(
            si->dots[i],
            DOT_SIZE,
            DOT_SIZE);

        lv_obj_set_style_radius(
            si->dots[i],
            LV_RADIUS_CIRCLE,
            0);

        int32_t a =
            i * 3600 / DOT_COUNT;

        int32_t x =
            lv_trigo_cos(a) *
            DOT_RADIUS /
            LV_TRIGO_SIN_MAX;

        int32_t y =
            lv_trigo_sin(a) *
            DOT_RADIUS /
            LV_TRIGO_SIN_MAX;

        lv_obj_align(
            si->dots[i],
            LV_ALIGN_CENTER,
            x,
            y);

        lv_obj_add_flag(
            si->dots[i],
            LV_OBJ_FLAG_HIDDEN);
    }
}

void statusIndicatorShow(
    StatusIndicator *si,
    StatusIndicatorState state)
{
    statusIndicatorHide(si);
    LV_SHOW(si->root);
    si->state = state;
    for(int i=0 ; i < DOT_COUNT ; i++) {
        LV_HIDE(si->dots[i]);
    }
    switch(state) {
    case STATUS_INDICATOR_WAITING:
        setColor(
            si,
            MAIN_THEME_COLOR);
        for(int i=0 ; i < DOT_COUNT ; i++) {
            LV_SHOW(si->dots[i]);
        }

        si->waitTimer =
            lv_timer_create(
                waitTimerCb,
                WAIT_PERIOD,
                si);
        lv_label_set_text(
            si->icon,
            "");
        break;
    case STATUS_INDICATOR_SUCCESS:
        setColor(
            si,
            MAIN_THEME_COLOR);
        lv_label_set_text(
            si->icon,
            LV_SYMBOL_OK);

        startRipple(
            si->ripple1,
            0);

        startRipple(
            si->ripple2,
            RIPPLE_DELAY);

        break;
    case STATUS_INDICATOR_ERROR:
        setColor(
            si,
            MAIN_THEME_COLOR);
        lv_label_set_text(
            si->icon,
            LV_SYMBOL_CLOSE);

        startRipple(
            si->ripple1,
            0);

        startRipple(
            si->ripple2,
            RIPPLE_DELAY);

        break;
    case STATUS_INDICATOR_WARNING:
        setColor(
            si,
            MAIN_THEME_COLOR);
        lv_label_set_text(
            si->icon,
            LV_SYMBOL_WARNING);

        break;
    }
    lv_obj_move_foreground(si->icon);
}

void statusIndicatorHide(
    StatusIndicator *si)
{
    lv_anim_del(
        si->ripple1,
        NULL);

    lv_anim_del(
        si->ripple2,
        NULL);

    if(si->waitTimer)
    {
        lv_timer_del(
            si->waitTimer);

        si->waitTimer = NULL;
    }
    LV_HIDE(si->root);

    si->state = STATUS_INDICATOR_HIDDEN;
}

void statusIndicatorAlign(
    StatusIndicator *si, lv_align_t align,
    int32_t x,
    int32_t y)
{
    if(si == NULL)
        return;
    lv_obj_align(si->root, align, x, y);
}