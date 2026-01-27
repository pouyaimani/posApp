#include "config.h"
#include "display.h"
#include "../hal/dev/dev.h"
#include "logger.h"

#define SCREEN_SIZE         DISP_HOR_RES * DISP_VER_RES
#define BYTES_PER_PIXEL     2

static Display display;
static Device *dev;
// Display buffer
static uint8_t *buffer;
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */
#define LV_BUFFER_SIZE  (DISP_HOR_RES * 60) * BYTE_PER_PIXEL

#define STATUS_BAR_WIDTH DISP_HOR_RES
#define STATUS_BAR_HEIGHT 30

static volatile bool isFlushEnabled;

static lv_display_t *lv_disp;

static void initBuffer() {
    buffer = GET_MEM(LV_BUFFER_SIZE);
    LV_ASSERT_MALLOC(buffer);
    memset(buffer, 0, LV_BUFFER_SIZE);
}

static void dispFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *cmap)
{
    OOP_CALL(dev, flushDisplay, area->x1, area->x2, area->y1, area->y2, cmap);
    // Inform the graphics library that you are ready with the flushing
    lv_display_flush_ready(disp);
}


#if LV_USE_LOG
void lvLogCb(lv_log_level_t level, const char * buf) {
    OOP_CALL(dev, logOut, buf, 0, NULL);
}
#endif

static void displayInit() { 
    LOG_TRACE("Initializing display starts ...");

    initBuffer();
    // init LVGL
    LOG_TRACE("Initializing LVGL ...");
#if LV_USE_LOG
    lv_log_register_print_cb(lvLogCb);
#endif
    lv_init();

    LOG_TRACE("Creating display ...");
    lv_disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_default(lv_disp);
    lv_display_set_buffers(lv_disp, buffer, NULL, (DISP_HOR_RES * 60) , LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, dispFlush);

    LOG_TRACE("Loading main screen ...");
    display.fscreen = lv_obj_create(NULL);
    lv_screen_load(display.fscreen);
    lv_obj_set_size(display.fscreen, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_set_style_bg_color(display.fscreen, lv_color_white(), 0);
    lv_obj_set_scrollbar_mode(display.fscreen, LV_SCROLLBAR_MODE_OFF);

    display.statusbar = lv_obj_create(display.fscreen);
    lv_obj_set_size(display.statusbar, STATUS_BAR_WIDTH + 10, STATUS_BAR_HEIGHT + 10);
    lv_obj_set_style_bg_color(display.statusbar,lv_color_hex(MAIN_THEME_COLOR), 0);
    lv_obj_align(display.statusbar, LV_ALIGN_TOP_MID, 5, -10);
    lv_obj_set_style_radius(display.statusbar, 10, 0);
    lv_obj_set_scrollbar_mode(display.statusbar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_shadow_opa(display.statusbar, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(display.statusbar, lv_color_black(), 0);
    lv_obj_set_style_shadow_offset_x(display.statusbar, -4, 0);
    lv_obj_set_style_shadow_offset_y(display.statusbar, 4, 0);
    lv_obj_set_style_shadow_spread(display.statusbar, 0, 0);
    lv_obj_set_style_shadow_width(display.statusbar, 4, 0);

    display.screen = lv_obj_create(display.fscreen);
    lv_obj_set_size(display.screen, DISP_HOR_RES, DISP_VER_RES - STATUS_BAR_HEIGHT);
    lv_obj_set_style_bg_color(display.screen, lv_color_white(), 0);
    lv_obj_align(display.screen, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_scrollbar_mode(display.screen, LV_SCROLLBAR_MODE_OFF);

    LOG_TRACE("Initializing display finished ...");
}

static void displayUpdate() {
    // Previous tick
    static uint32_t ptick = 0;
    CALL_ONCE(
        ptick = GET_TICK();
    );
    // Current tick
    uint32_t ctick = GET_TICK();
    uint32_t elapsed = ctick - ptick;
    // LOG_TRACE("elapsed time = %d", elapsed);
    lv_tick_inc(elapsed);
    ptick = ctick;
    // LVGL timer handler
    lv_task_handler();
}

OOP_CTOR(Display) {
    LOG_TRACE("Display constructor ...");
    self->init = displayInit;
    self->update = displayUpdate;
    dev = getDevice();
}

Display *getDisplay(void) {
    CALL_ONCE(
        OOP_CALL_CTOR(Display, &display);
    );
    return &display;
}