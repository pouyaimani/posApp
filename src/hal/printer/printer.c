#include "printer.h"

#ifdef DEVICE_TRENDITT3RTOS
#endif

static Printer *printer;

void Printer_ctor(Printer* self) {
}

Printer *getPrinter() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
#endif
    );
    return printer;
}