#ifdef DEVICE_TRENDITT3RTOS

#include "printer_t3Rtos.h"
#include "posplatform.h"

static void PrinterT3Rtos_init(PrinterT3Rtos* dev) {
}


void PrinterT3Rtos_ctor(PrinterT3Rtos* self, const char* name) {
    self->base.vtable.init = PrinterT3Rtos_init;
    self->base.name = name;
}

#endif