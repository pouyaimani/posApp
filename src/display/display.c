#include "config.h"
#include "display.h"
#include "../hal/dev/dev.h"

#define SCREEN_SIZE         DISP_HOR_RES * DISP_VER_RES
#define BYTES_PER_PIXEL     2

static Display display;
static Device *dev;
// Display buffer
static uint8_t *buffer;
#define LV_BUFFER_SIZE  (SCREEN_SIZE / 10) * BYTES_PER_PIXEL
// Previous tick
static uint32_t ptick = 0;

static volatile bool isFlushEnabled;

static lv_display_t *lv_disp;

static void initBuffer() {
    buffer = OOP_CALL(dev, getMemory, LV_BUFFER_SIZE);
    memset(buffer, 0, LV_BUFFER_SIZE);
}

static void disp_flush(lv_display_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    OOP_CALL(dev, flushDisplay, area->x1, area->x2, area->y1, area->y2, color_p);
    // Inform the graphics library that you are ready with the flushing
    lv_display_flush_ready(disp);
}

static void displayInit(Display* disp) { 

    initBuffer();
    // init LVGL
    lv_init();

    lv_disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_buffers(lv_disp, buffer, NULL, LV_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_flush_cb(lv_disp, disp_flush);

    // main screen
    disp->screen = lv_obj_create(NULL);
    lv_screen_load(disp->screen);

}

static void displayUpdate(Display* ui) {
    // Current tick
    uint32_t ctick = OOP_CALL(dev, getTick);
    lv_tick_inc(ctick - ptick);
    ptick = ctick;

    // LVGL timer handler
    lv_timer_handler();
}

OOP_CTOR(Display) {
    self->init = displayInit;
    self->update = displayUpdate;
    dev = getDevice();
}

Display *getDisplay(void) {
    CALL_ONCE(
        Display_ctor(&display);
    );
    return &display;
}