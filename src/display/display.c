#include "display.h"
#include "../hal/dev/dev.h"

static Display display;
static Device *dev;
// Previous tick
static uint32_t ptick = 0;

static void displayInit(Display* ui) { 
    // init LVGL
    lv_init();
}

static void displayUpdate(Display* ui) {
    // Current tick
    uint32_t ctick = OOP_CALL(dev, getTick);
    lv_tick_inc(ctick - ptick);
    ptick = ctick;

    // LVGL timer handler
    lv_timer_handler();
}

void Display_ctor(Display* self, const char* name) {
    self->init = displayInit;
    self->update = displayUpdate;
    dev = getDevice();
}

Display *getDisplay(void) {
    CALL_ONCE(
        Display_ctor(&display, "");
    );
    return &display;
}