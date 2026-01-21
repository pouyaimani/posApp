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

static volatile bool isFlushEnabled;

static lv_display_t *lv_disp;

static void initBuffer() {
    buffer = GET_MEM(LV_BUFFER_SIZE);
    LV_ASSERT_MALLOC(buffer);
    memset(buffer, 0, LV_BUFFER_SIZE);
}

static void dispFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *cmap)
{
    LOG_TRACE("x0 = %d , x1 = %d , y0 = %d , y1 = %d \n", area->x1, area->x2, area->y1, area->y2);
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
    display.screen = lv_obj_create(NULL);
    lv_screen_load(display.screen);
    lv_obj_set_size(display.screen, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_set_style_bg_color(display.screen, lv_color_hex(0xe32400), 0);
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