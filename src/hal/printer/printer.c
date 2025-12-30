#include "printer.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/printer_t3Rtos.h"
#endif

static Printer *printer;

void Printer_ctor(Printer* self) {
}

Printer *getPrinter() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    DEVICE_REGISTER(Printer, PrinterT3Rtos, printer)
#else
#error Deivce printer is undefined. Make sure correct device is chosen and its printer driver is developed.
#endif
    );
    return printer;
}