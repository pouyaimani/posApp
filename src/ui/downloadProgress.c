#include "downloadProgress.h"
#include "sys/sys.h"

#include <stdio.h>
#include <stdlib.h>

#define DOWNLOAD_CARD_HEIGHT 100

#define DOWNLOAD_CARD_PADDING_X 12
#define DOWNLOAD_CARD_PADDING_Y 10

#define DOWNLOAD_BAR_HEIGHT 12

#define DOWNLOAD_TITLE_HEIGHT  20
#define DOWNLOAD_PERCENT_WIDTH 45
#define DOWNLOAD_SIZE_HEIGHT   18

DownloadProgress* ui_download_progress_create(lv_obj_t* parent, int x, int y,
                                              int width) {
    if (!parent)
        return NULL;

    DownloadProgress* progress =
        (DownloadProgress*)MEM_ALLOC(sizeof(DownloadProgress));

    if (!progress)
        return NULL;

    progress->container = NULL;
    progress->title     = NULL;
    progress->bar       = NULL;
    progress->percent   = NULL;
    progress->sizeLabel = NULL;
    progress->value     = 0;
    progress->max       = 100;

    /* ============================================================
     * Container
     * ============================================================ */

    progress->container = lv_obj_create(parent);

    lv_obj_set_size(progress->container, width, DOWNLOAD_CARD_HEIGHT);

    lv_obj_set_pos(progress->container, x, y);

    lv_obj_clear_flag(progress->container, LV_OBJ_FLAG_SCROLLABLE);

    /* Card background */

    lv_obj_set_style_bg_color(progress->container, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);

    lv_obj_set_style_bg_opa(progress->container, LV_OPA_COVER, LV_PART_MAIN);

    /* Rounded card */

    lv_obj_set_style_radius(progress->container, 10, LV_PART_MAIN);

    /* Thin border */

    lv_obj_set_style_border_width(progress->container, 1, LV_PART_MAIN);

    lv_obj_set_style_border_color(progress->container, lv_color_hex(0xE6E8EC),
                                  LV_PART_MAIN);

    /* Padding */

    lv_obj_set_style_pad_all(progress->container, 0, LV_PART_MAIN);

    /* ============================================================
     * Title
     * ============================================================ */

    progress->title = lv_label_create(progress->container);

    lv_label_set_text(progress->title, "Downloading update...");

    lv_obj_set_pos(progress->title, DOWNLOAD_CARD_PADDING_X,
                   DOWNLOAD_CARD_PADDING_Y);

    lv_obj_set_size(progress->title, width - (DOWNLOAD_CARD_PADDING_X * 2),
                    DOWNLOAD_TITLE_HEIGHT);

    lv_obj_set_style_text_color(progress->title, lv_color_hex(0x25282D),
                                LV_PART_MAIN);

    /* ============================================================
     * Progress bar
     * ============================================================ */

    progress->bar = lv_bar_create(progress->container);

    lv_obj_set_size(progress->bar, width - (DOWNLOAD_CARD_PADDING_X * 2),
                    DOWNLOAD_BAR_HEIGHT);

    lv_obj_set_pos(progress->bar, DOWNLOAD_CARD_PADDING_X, 40);

    lv_bar_set_range(progress->bar, 0, 100);

    lv_bar_set_value(progress->bar, 0, LV_ANIM_OFF);

    /* Background / unfilled part */

    lv_obj_set_style_bg_color(progress->bar, lv_color_hex(0xE9EDF2),
                              LV_PART_MAIN);

    lv_obj_set_style_bg_opa(progress->bar, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_radius(progress->bar, DOWNLOAD_BAR_HEIGHT / 2,
                            LV_PART_MAIN);

    /* Filled part */

    lv_obj_set_style_bg_color(progress->bar, lv_color_hex(0x3478F6),
                              LV_PART_INDICATOR);

    lv_obj_set_style_bg_opa(progress->bar, LV_OPA_COVER, LV_PART_INDICATOR);

    lv_obj_set_style_radius(progress->bar, DOWNLOAD_BAR_HEIGHT / 2,
                            LV_PART_INDICATOR);

    /* ============================================================
     * Percentage
     * ============================================================ */

    progress->percent = lv_label_create(progress->container);

    lv_label_set_text(progress->percent, "0%");

    lv_obj_set_size(progress->percent, DOWNLOAD_PERCENT_WIDTH, 20);

    lv_obj_align(progress->percent, LV_ALIGN_BOTTOM_RIGHT,
                 -DOWNLOAD_CARD_PADDING_X, -8);

    lv_obj_set_style_text_align(progress->percent, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN);

    lv_obj_set_style_text_color(progress->percent, lv_color_hex(0x3478F6),
                                LV_PART_MAIN);

    /* ============================================================
     * Download size
     * ============================================================ */

    progress->sizeLabel = lv_label_create(progress->container);

    lv_label_set_text(progress->sizeLabel, "");

    lv_obj_set_size(progress->sizeLabel,
                    width - DOWNLOAD_PERCENT_WIDTH -
                        DOWNLOAD_CARD_PADDING_X * 3,
                    DOWNLOAD_SIZE_HEIGHT);

    lv_obj_align(progress->sizeLabel, LV_ALIGN_BOTTOM_LEFT,
                 DOWNLOAD_CARD_PADDING_X, -8);

    lv_obj_set_style_text_color(progress->sizeLabel, lv_color_hex(0x80858D),
                                LV_PART_MAIN);

    return progress;
}

void ui_download_progress_set_value(DownloadProgress* progress, uint32_t value,
                                    uint32_t max) {
    if (!progress || !progress->bar)
        return;

    if (max == 0)
        return;

    if (value > max)
        value = max;

    progress->value = value;
    progress->max   = max;

    uint32_t percent = (value * 100U) / max;

    lv_bar_set_value(progress->bar, percent, LV_ANIM_ON);

    char buffer[16];

    snprintf(buffer, sizeof(buffer), "%lu%%", (unsigned long)percent);

    lv_label_set_text(progress->percent, buffer);
}

void ui_download_progress_set_size(DownloadProgress* progress,
                                   uint32_t downloadedKB, uint32_t totalKB) {
    if (!progress || !progress->sizeLabel)
        return;

    char buffer[48];

    if (totalKB >= 1024) {

        uint32_t downloadedWhole = downloadedKB / 1024;

        uint32_t downloadedDecimal = ((downloadedKB % 1024) * 10) / 1024;

        uint32_t totalWhole = totalKB / 1024;

        uint32_t totalDecimal = ((totalKB % 1024) * 10) / 1024;

        snprintf(buffer, sizeof(buffer), "%lu.%lu MB / %lu.%lu MB",
                 (unsigned long)downloadedWhole,
                 (unsigned long)downloadedDecimal, (unsigned long)totalWhole,
                 (unsigned long)totalDecimal);

    } else {

        snprintf(buffer, sizeof(buffer), "%lu KB / %lu KB",
                 (unsigned long)downloadedKB, (unsigned long)totalKB);
    }

    lv_label_set_text(progress->sizeLabel, buffer);
}

void ui_download_progress_show(DownloadProgress* progress, bool show) {
    if (!progress || !progress->sizeLabel)
        return;

    if (show) {

        lv_obj_clear_flag(progress->sizeLabel, LV_OBJ_FLAG_HIDDEN);

    } else {

        lv_obj_add_flag(progress->sizeLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_download_progress_destroy(DownloadProgress* progress) {
    if (!progress)
        return;

    if (progress->container) {

        lv_obj_delete(progress->container);

        progress->container = NULL;
    }

    MEM_FREE(progress);
}

void ui_download_progress_set_title(DownloadProgress* progress,
                                    const char*       text) {
    if (!progress || !progress->title || !text)
        return;

    lv_label_set_text(progress->title, text);
}