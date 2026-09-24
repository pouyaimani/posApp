#ifndef DOWNLOAD_PROGRESS_H
#define DOWNLOAD_PROGRESS_H

#include "lvgl.h"
#include <stdint.h>

typedef struct {
    lv_obj_t* container;
    lv_obj_t* title;
    lv_obj_t* bar;
    lv_obj_t* percent;
    lv_obj_t* sizeLabel;

    uint32_t value;
    uint32_t max;
} DownloadProgress;

/**
 * Create download progress widget.
 *
 * parent : LVGL parent object
 * x/y    : position
 * width  : widget width
 */
DownloadProgress* ui_download_progress_create(lv_obj_t* parent, int x, int y,
                                              int width);

/**
 * Set download progress.
 *
 * value = downloaded amount
 * max   = total amount
 *
 * Example:
 * ui_download_progress_set_value(progress, 64, 100);
 */
void ui_download_progress_set_value(DownloadProgress* progress, uint32_t value,
                                    uint32_t max);

/**
 * Set title.
 *
 * Example:
 * ui_download_progress_set_title(progress, "Downloading update...");
 */
void ui_download_progress_set_title(DownloadProgress* progress,
                                    const char*       text);

/**
 * Optional size information.
 *
 * Example:
 * ui_download_progress_set_size(progress,
 *                               2300,
 *                               3600);
 *
 * Result:
 * 2.3 MB / 3.6 MB
 */
void ui_download_progress_set_size(DownloadProgress* progress,
                                   uint32_t downloadedKB, uint32_t totalKB);

/**
 * Hide/show size text.
 */
void ui_download_progress_show(DownloadProgress* progress, bool show);

/**
 * Destroy widget.
 */
void ui_download_progress_destroy(DownloadProgress* progress);

#endif