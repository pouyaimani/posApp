#include "infoPage.h"
#include "display/display.h"
#include "font/myFont.h"
#include "common.h"
#include "logger.h"

#define INFO_ICON_Y_OFFSET      (-60)

#define ORBIT_RADIUS            30
#define ORBIT_DOT_SIZE          8
#define ORBIT_DURATION_MS       2000

#define RIPPLE_START_SIZE       30
#define RIPPLE_END_SIZE         140
#define RIPPLE_DURATION_MS      600
#define RIPPLE_DELAY_MS         120

#define INFO_COUNT (INFO_IMG + 1)

static InfoTheme_t themes[INFO_COUNT];

static void initThemes(void)
{
    themes[INFO_SUCCESS] = (InfoTheme_t) {
        .color  = lv_palette_main(LV_PALETTE_GREEN),
        .symbol = LV_SYMBOL_OK,
    };

    themes[INFO_ERROR] = (InfoTheme_t) {
        .color  = lv_palette_main(LV_PALETTE_RED),
        .symbol = LV_SYMBOL_CLOSE,
    };

    themes[INFO_WARNING] = (InfoTheme_t) {
        .color  = lv_palette_main(LV_PALETTE_ORANGE),
        .symbol = LV_SYMBOL_WARNING,
    };

    themes[INFO_WAITING] = (InfoTheme_t) {
        .color  = lv_palette_main(LV_PALETTE_PURPLE),
        .symbol = "",
    };
}

static void ripple_size_anim_cb(
    void *obj,
    int32_t v) {
    lv_obj_set_size(obj, v, v);

    lv_obj_align(
        obj,
        LV_ALIGN_CENTER,
        0,
        INFO_ICON_Y_OFFSET);
}

static void ripple_opa_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_border_opa(obj, v, 0);
}

static void orbit_anim_cb(
    void *var,
    int32_t angle)
{
    OrbitCtx *ctx = var;

    int32_t cx =
        lv_obj_get_x(ctx->circle) +
        lv_obj_get_width(ctx->circle) / 2;

    int32_t cy =
        lv_obj_get_y(ctx->circle) +
        lv_obj_get_height(ctx->circle) / 2;

    int32_t x =
        cx +
        lv_trigo_cos(angle) *
        ORBIT_RADIUS /
        LV_TRIGO_SIN_MAX;

    int32_t y =
        cy +
        lv_trigo_sin(angle) *
        ORBIT_RADIUS /
        LV_TRIGO_SIN_MAX;

    lv_obj_set_pos(
        ctx->dot,
        x - ORBIT_DOT_SIZE / 2,
        y - ORBIT_DOT_SIZE / 2);
}

static void stopAnimations(
    InfoPage *pinfo)
{
    lv_anim_del(
        &pinfo->anim.orbit,
        NULL);

    lv_anim_del(
        pinfo->anim.ripple1,
        NULL);

    lv_anim_del(
        pinfo->anim.ripple2,
        NULL);
}

static lv_obj_t *create_ripple(
    lv_obj_t *parent,
    lv_color_t color) {
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);

    lv_obj_set_size(obj, 40, 40);

    lv_obj_set_style_radius(
        obj,
        LV_RADIUS_CIRCLE,
        0);

    lv_obj_set_style_bg_opa(
        obj,
        LV_OPA_TRANSP,
        0);

    lv_obj_set_style_border_width(
        obj,
        2,
        0);

    lv_obj_set_style_border_color(
        obj,
        color,
        0);

    LV_ALIGN(obj, LV_ALIGN_CENTER, 0, INFO_ICON_Y_OFFSET);

    return obj;
}

static void start_ripple(
    lv_obj_t *obj,
    uint32_t delay)
{
    lv_anim_t a;
        lv_anim_init(&a);

    lv_anim_set_var(&a, obj);

    lv_anim_set_exec_cb(
        &a,
        ripple_size_anim_cb);

    lv_anim_set_values(
        &a,
        RIPPLE_START_SIZE,
        RIPPLE_END_SIZE);

    lv_anim_set_time(
        &a,
        RIPPLE_DURATION_MS);

    lv_anim_set_delay(
        &a,
        delay);

    lv_anim_start(&a);

        lv_anim_init(&a);

    lv_anim_set_var(&a, obj);

    lv_anim_set_exec_cb(
        &a,
        ripple_opa_anim_cb);

    lv_anim_set_values(
        &a,
        LV_OPA_COVER,
        LV_OPA_TRANSP);

    lv_anim_set_time(
        &a,
        RIPPLE_DURATION_MS);

    lv_anim_set_delay(
        &a,
        delay);

    lv_anim_start(&a);
}

static void start_orbitDot(
    InfoPage *pinfo)
{
    lv_anim_t a;

    lv_anim_init(&a);

    lv_anim_set_var(
        &a,
        &pinfo->anim.orbit);

    lv_anim_set_exec_cb(
        &a,
        orbit_anim_cb);

    lv_anim_set_values(
        &a,
        0,
        360);

    lv_anim_set_time(
        &a,
        ORBIT_DURATION_MS);

    lv_anim_set_repeat_count(
        &a,
        LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&a);
}

static void createAnim(InfoPage *pinfo) {
    pinfo->anim.circle = lv_obj_create(pinfo->parent);

    lv_obj_remove_style_all(pinfo->anim.circle);

    lv_obj_set_size(pinfo->anim.circle, RIPPLE_START_SIZE, RIPPLE_START_SIZE);

    lv_obj_set_style_radius(
        pinfo->anim.circle,
        LV_RADIUS_CIRCLE,
        0);

    lv_obj_set_style_bg_opa(
        pinfo->anim.circle,
        LV_OPA_20,
        0);

    LV_ALIGN(pinfo->anim.circle, LV_ALIGN_CENTER, 0, INFO_ICON_Y_OFFSET);

    pinfo->anim.icon = lv_label_create(pinfo->parent);
    
    LV_ALIGN(pinfo->anim.icon, LV_ALIGN_CENTER, 0, INFO_ICON_Y_OFFSET);

    pinfo->anim.orbit_dot = lv_obj_create(pinfo->parent);

    lv_obj_remove_style_all(pinfo->anim.orbit_dot);

    lv_obj_set_size(
        pinfo->anim.orbit_dot,
        ORBIT_DOT_SIZE,
        ORBIT_DOT_SIZE);

    lv_obj_set_style_radius(
        pinfo->anim.orbit_dot,
        LV_RADIUS_CIRCLE,
        0);
    lv_obj_set_style_bg_color(
        pinfo->anim.orbit_dot,
        lv_color_hex(0xFF0000),
        0);

    lv_obj_set_style_bg_opa(
        pinfo->anim.orbit_dot,
        LV_OPA_COVER,
        0);

    pinfo->anim.orbit.circle =
                pinfo->anim.circle;

    pinfo->anim.orbit.dot =
                pinfo->anim.orbit_dot;

    pinfo->anim.ripple1 =
        create_ripple(pinfo->parent, lv_color_white());

    pinfo->anim.ripple2 =
        create_ripple(pinfo->parent, lv_color_white());
}

static void startStateAnimation(
    InfoPage *pinfo)
{
    stopAnimations(pinfo);

    switch(pinfo->type)
    {
    case INFO_WAITING:

        LV_SHOW(pinfo->anim.orbit_dot);

        start_orbitDot(pinfo);

        break;

    case INFO_SUCCESS:
    case INFO_ERROR:
    case INFO_WARNING:

        LV_HIDE(pinfo->anim.orbit_dot);

        start_ripple(
            pinfo->anim.ripple1,
            0);

        start_ripple(
            pinfo->anim.ripple2,
            RIPPLE_DELAY_MS);

        break;

    default:
        break;
    }
}

static void hideAnim(InfoPage *pinfo) {
    stopAnimations(pinfo);
    LV_HIDE(pinfo->anim.orbit_dot);
    LV_HIDE(pinfo->anim.circle);
    LV_HIDE(pinfo->anim.icon);
    LV_HIDE(pinfo->anim.ripple1);
    LV_HIDE(pinfo->anim.ripple2);
    // LV_HIDE(pinfo->anim.title);
    LV_HIDE(pinfo->body);
    LV_HIDE(pinfo->title);
}

static void showAnim(InfoPage *pinfo) {
    if (pinfo->type == INFO_WAITING) {
        LV_SHOW(pinfo->anim.orbit_dot);
        LV_HIDE(pinfo->anim.icon);
        LV_HIDE(pinfo->anim.ripple1);
        LV_HIDE(pinfo->anim.ripple2);
    } else {
        LV_SHOW(pinfo->anim.icon);
        LV_SHOW(pinfo->anim.ripple1);
        LV_SHOW(pinfo->anim.ripple2);
        LV_HIDE(pinfo->anim.orbit_dot);
    }
    LV_SHOW(pinfo->body);
    LV_SHOW(pinfo->title);
    LV_SHOW(pinfo->anim.circle);
    startStateAnimation(pinfo);
}

static void setTheme(InfoPage *pinfo, InfoTheme_t *theme) {
    lv_obj_set_style_bg_color(
        pinfo->anim.circle,
        theme->color,
        0);
    lv_label_set_text(
        pinfo->anim.icon,
        theme->symbol);

    lv_obj_set_style_text_color(
        pinfo->anim.icon,
        theme->color,
        0);
    lv_obj_set_style_border_color(
        pinfo->anim.ripple1,
        theme->color,
        0);
    lv_obj_set_style_border_color(
        pinfo->anim.ripple2,
        theme->color,
        0);
    lv_obj_set_style_bg_color(
        pinfo->anim.orbit_dot,
        theme->color,
        0);
}

static void createInfoPage(InfoPage *pinfo) {
    RETURN_IF_NULL(pinfo, ;);
    pinfo->parent = disp()->screen;
    createAnim(pinfo);

    pinfo->title = lv_label_create(pinfo->parent);
    LV_SET_TEXT_FONT(pinfo->title, FONT_20);
    LV_SET_TEXT_COLOR(pinfo->title, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->title, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->title, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->title, LV_ALIGN_CENTER, 0, 0);
    
    pinfo->body = lv_label_create(pinfo->parent);
    LV_SET_TEXT_FONT(pinfo->body, FONT_16);
    LV_SET_TEXT_COLOR(pinfo->body, COLOR_BLACK);
    LV_SET_TEXT_ALIGN(pinfo->body, LV_TEXT_ALIGN_CENTER);
    LV_SET_SIZE(pinfo->body, lv_pct(90), LV_SIZE_CONTENT);
    LV_ALIGN(pinfo->body, LV_ALIGN_CENTER, 0, 30);
    
    pinfo->img = NULL;

    pinfo->line = lv_obj_create(pinfo->parent);
    LV_SET_SIZE(pinfo->line, 190, 8);
    LV_ALIGN(pinfo->line, LV_ALIGN_TOP_MID, 0, 140);
    LV_SET_RADIUS(pinfo->line, 17);
    LV_SET_BG_COLOR(pinfo->line, 0x333333);
    LV_SET_BORDER_COLOR(pinfo->line, 0x333333);

    LV_SET_TEXT(pinfo->title, "");
    LV_SET_TEXT(pinfo->body, "");
    hideAnim(pinfo);
}

static void infoHide(InfoPage *pinfo) {
    RETURN_IF_NULL(pinfo, ;);
    hideAnim(pinfo);
    if(pinfo->img) {
        LV_HIDE(pinfo->img);
        LV_DELETE(pinfo->img);
        pinfo->img = NULL;
    }
    LV_HIDE(pinfo->line);
}

static void infoShow(InfoPage *pinfo) {
    RETURN_IF_NULL(pinfo, ;);
    LOG_DEBUG("pinfo->type = %d", pinfo->type);
    if (pinfo->type == INFO_IMG) {
        LV_SHOW(pinfo->body);
        if (pinfo->img) {
            LV_SHOW(pinfo->img);
        }
        LV_SHOW(pinfo->line);
        LV_ALIGN(pinfo->body, LV_ALIGN_TOP_MID, 0, 161);
    } else {
        setTheme(pinfo, &themes[pinfo->type]);
        showAnim(pinfo);
    }
}

static void infoSetData(InfoPage *pinfo, InfoType_t type, const char *data, const char* body) {
    RETURN_IF_NULL(pinfo, ;);
    pinfo->type = type;
    if (type == INFO_IMG) {
        if(pinfo->img) {
            lv_obj_del(pinfo->img);
            pinfo->img = NULL;
        }
        pinfo->img = lv_img_create(disp()->screen);
        LV_ALIGN(pinfo->img, LV_ALIGN_TOP_MID, 0, 36);
        LV_SCROLL_DISABLE(pinfo->img);
        LV_CLICK_DISABLE(pinfo->img);
        lv_img_set_src(pinfo->img, data);
        LV_SET_TEXT(pinfo->body, body);
    } else {
        LV_SET_TEXT(pinfo->title, data);
        LV_SET_TEXT(pinfo->body, body);
    }
    // TODO: force update lvgl
}

InfoPage *infoPage() {
    static InfoPage info;
    CALL_ONCE(
        initThemes();
        createInfoPage(&info);
        info.vtable.hide = infoHide;
        info.vtable.show = infoShow;
        info.vtable.setData = infoSetData;
        infoHide(&info);
    );
    return &info;
}