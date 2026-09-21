#include "progressBar.h"

#include "display.h"
#include "utility/utility.h"
#include "common.h"
#include "font/myFont.h"

static lv_style_t style_bg;
static lv_style_t style_indic;

static bool styleInitialized = false;

static void initStyles(void) {
    if (styleInitialized)
        return;

    /* Background */
    lv_style_init(&style_bg);

    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0xE8E8E8));

    lv_style_set_border_color(&style_bg, lv_color_hex(MAIN_THEME_COLOR));

    lv_style_set_border_width(&style_bg, 2);

    lv_style_set_radius(&style_bg, 6);

    /*
     * Gives some space between outer border
     * and actual progress indicator.
     */
    lv_style_set_pad_all(&style_bg, 4);

    /* Indicator */
    lv_style_init(&style_indic);

    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);

    lv_style_set_bg_color(&style_indic, lv_color_hex(MAIN_THEME_COLOR));

    lv_style_set_radius(&style_indic, 3);

    styleInitialized = true;
}

void ui_progress_bar_create(ProgressBar* progress, lv_obj_t* parent) {
    RETURN_IF_NULL(progress, ;);
    RETURN_IF_NULL(parent, ;);

    initStyles();

    progress->value = 0;

    /*
     * Title
     */
    progress->title = lv_label_create(parent);

    LV_SET_TEXT_FONT(progress->title, FONT_20);

    LV_SET_TEXT_COLOR(progress->title, COLOR_BLACK);

    LV_SET_TEXT_ALIGN(progress->title, LV_TEXT_ALIGN_CENTER);

    LV_SET_SIZE(progress->title, lv_pct(90), LV_SIZE_CONTENT);

    LV_ALIGN(progress->title, LV_ALIGN_TOP_MID, 0, 50);

    /*
     * Progress bar
     */
    progress->bar = lv_bar_create(parent);

    lv_obj_remove_style_all(progress->bar);

    lv_obj_add_style(progress->bar, &style_bg, 0);

    lv_obj_add_style(progress->bar, &style_indic, LV_PART_INDICATOR);

    lv_obj_set_size(progress->bar, lv_pct(80), 32);

    lv_obj_align(progress->bar, LV_ALIGN_CENTER, 0, 0);

    lv_bar_set_range(progress->bar, 0, 100);

    /*
     * Percentage
     */
    progress->valueLabel = lv_label_create(parent);

    LV_SET_TEXT_FONT(progress->valueLabel, FONT_20);

    LV_SET_TEXT_COLOR(progress->valueLabel, COLOR_BLACK);

    LV_SET_TEXT_ALIGN(progress->valueLabel, LV_TEXT_ALIGN_CENTER);

    lv_obj_set_width(progress->valueLabel, lv_pct(80));

    lv_obj_align_to(progress->valueLabel, progress->bar,
                    LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    ui_progress_bar_set_value(progress, 0);
}

void ui_progress_bar_set_value(ProgressBar* progress, int value) {
    RETURN_IF_NULL(progress, ;);

    if (value < 0)
        value = 0;

    if (value > 100)
        value = 100;

    progress->value = value;

    lv_bar_set_value(progress->bar, value, LV_ANIM_ON);

    if (progress->valueLabel) {
        lv_label_set_text_fmt(progress->valueLabel, "%d%%", value);
    }
}

void ui_progress_bar_set_title(ProgressBar* progress, const char* title) {
    RETURN_IF_NULL(progress, ;);
    RETURN_IF_NULL(title, ;);

    LV_SET_TEXT(progress->title, title);
}

void ui_progress_bar_show(ProgressBar* progress) {
    RETURN_IF_NULL(progress, ;);

    LV_SHOW(progress->bar);
    LV_SHOW(progress->title);
    LV_SHOW(progress->valueLabel);
}

void ui_progress_bar_hide(ProgressBar* progress) {
    RETURN_IF_NULL(progress, ;);

    LV_HIDE(progress->bar);
    LV_HIDE(progress->title);
    LV_HIDE(progress->valueLabel);
}

void ui_progress_bar_destroy(ProgressBar* progress) {
    RETURN_IF_NULL(progress, ;);

    if (progress->bar) {
        lv_obj_delete(progress->bar);
        progress->bar = NULL;
    }

    if (progress->title) {
        lv_obj_delete(progress->title);
        progress->title = NULL;
    }

    if (progress->valueLabel) {
        lv_obj_delete(progress->valueLabel);
        progress->valueLabel = NULL;
    }
}