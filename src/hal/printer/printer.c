#include "printer.h"

Printer *__printer;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/printer_t3Rtos.h"

static void constructT3Rtos() {
    static PrinterT3Rtos obj;
    __printer = (Printer *)&obj;
    OOP_CALL_CTOR(Printer, __printer);
    OOP_CALL_CTOR(PrinterT3Rtos, &obj);
}

#endif

OOP_CTOR(Printer) {
}

Printer *getPrinter() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce printer is undefined. Make sure correct device is chosen and its printer driver is developed.
#endif
    );
    return __printer;
}