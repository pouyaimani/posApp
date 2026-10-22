
#include "progressBar.h"

#include "display.h"
#include "utility/utility.h"
#include "common.h"
#include "font/myFont.h"

#define PROGRESS_BAR_HEIGHT    40
#define PROGRESS_BAR_WIDTH_PCT 80
#define PROGRESS_ANIM_DURATION 1000

static lv_style_t style_bg;
static lv_style_t style_indic;

static bool styleInitialized = false;

/**
 * Initialize shared progress bar styles.
 *
 * Based on LVGL lv_example_bar_2.
 * Uses MAIN_THEME_COLOR instead of LV_PALETTE_BLUE.
 */
static void initStyles(void) {
    if (styleInitialized)
        return;

    lv_color_t themeColor = lv_color_hex(MAIN_THEME_COLOR);

    /* Background / track */
    lv_style_init(&style_bg);

    lv_style_set_border_color(&style_bg, themeColor);

    lv_style_set_border_width(&style_bg, 2);

    /* Make the indicator smaller than the track. */
    lv_style_set_pad_all(&style_bg, 6);

    lv_style_set_radius(&style_bg, 6);

    lv_style_set_anim_duration(&style_bg, PROGRESS_ANIM_DURATION);

    /* Indicator */
    lv_style_init(&style_indic);

    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);

    lv_style_set_bg_color(&style_indic, themeColor);

    lv_style_set_radius(&style_indic, 3);

    styleInitialized = true;
}

/**
 * Create the progress bar and labels.
 *
 * Layout:
 *
 *          Downloading update
 *
 *    +--------------------------+
 *    |  ============            |
 *    +--------------------------+
 *
 *                65%
 */
void ui_progress_bar_create(ProgressBar* progress, lv_obj_t* parent) {
    RETURN_IF_NULL(progress, ;);
    RETURN_IF_NULL(parent, ;);

    initStyles();

    progress->value      = -1;
    progress->bar        = NULL;
    progress->title      = NULL;
    progress->valueLabel = NULL;

    /* Title */
    progress->title = lv_label_create(parent);

    LV_SET_TEXT_FONT(progress->title, FONT_20);

    LV_SET_TEXT_COLOR(progress->title, COLOR_BLACK);

    LV_SET_TEXT_ALIGN(progress->title, LV_TEXT_ALIGN_CENTER);

    LV_SET_SIZE(progress->title, lv_pct(90), LV_SIZE_CONTENT);

    LV_ALIGN(progress->title, LV_ALIGN_TOP_MID, 0, 50);

    /* Progress bar */
    progress->bar = lv_bar_create(parent);

    /* Remove default LVGL styles. */
    lv_obj_remove_style_all(progress->bar);

    lv_obj_add_style(progress->bar, &style_bg, LV_PART_MAIN);

    lv_obj_add_style(progress->bar, &style_indic, LV_PART_INDICATOR);

    lv_obj_set_size(progress->bar, lv_pct(PROGRESS_BAR_WIDTH_PCT),
                    PROGRESS_BAR_HEIGHT);

    lv_obj_align(progress->bar, LV_ALIGN_CENTER, 0, 0);

    lv_bar_set_range(progress->bar, 0, 100);

    /* Percentage label */
    progress->valueLabel = lv_label_create(parent);

    LV_SET_TEXT_FONT(progress->valueLabel, FONT_20);

    LV_SET_TEXT_COLOR(progress->valueLabel, COLOR_BLACK);

    LV_SET_TEXT_ALIGN(progress->valueLabel, LV_TEXT_ALIGN_CENTER);

    lv_obj_set_width(progress->valueLabel, lv_pct(PROGRESS_BAR_WIDTH_PCT));

    lv_obj_align_to(progress->valueLabel, progress->bar,
                    LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    /* Set initial progress without animation. */
    ui_progress_bar_set_value(progress, 0);
}

/**
 * Set progress value from 0 to 100.
 *
 * Animates subsequent value changes.
 */
void ui_progress_bar_set_value(ProgressBar* progress, int value) {
    RETURN_IF_NULL(progress, ;);

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (progress->value == value)
        return;

    bool initialUpdate = (progress->value < 0);

    progress->value = value;

    if (progress->bar) {
        lv_bar_set_value(progress->bar, value,
                         initialUpdate ? LV_ANIM_OFF : LV_ANIM_ON);
    }

    if (progress->valueLabel) {
        lv_label_set_text_fmt(progress->valueLabel, "%d%%", value);
    }
}

/**
 * Set progress bar title.
 */
void ui_progress_bar_set_title(ProgressBar* progress, const char* title) {
    RETURN_IF_NULL(progress, ;);
    RETURN_IF_NULL(title, ;);

    if (progress->title)
        LV_SET_TEXT(progress->title, title);
}

/**
 * Show progress bar and associated labels.
 */
void ui_progress_bar_show(ProgressBar* progress) {
    RETURN_IF_NULL(progress, ;);

    if (progress->bar)
        LV_SHOW(progress->bar);

    if (progress->title)
        LV_SHOW(progress->title);

    if (progress->valueLabel)
        LV_SHOW(progress->valueLabel);
}

/**
 * Hide progress bar and associated labels.
 */
void ui_progress_bar_hide(ProgressBar* progress) {
    RETURN_IF_NULL(progress, ;);

    if (progress->bar)
        LV_HIDE(progress->bar);

    if (progress->title)
        LV_HIDE(progress->title);

    if (progress->valueLabel)
        LV_HIDE(progress->valueLabel);
}

/**
 * Destroy all progress bar objects.
 *
 * Shared static styles remain initialized.
 */
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

    progress->value = 0;
}
