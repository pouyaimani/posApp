#include "printer.h"
#include <stdbool.h>
#include "logger.h"
#include "sys/sys.h"

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

static PrinterErr_t print(uint8_t *src, uint16_t width, uint16_t height) {
    int reopened = false;
reopen:
    PrinterErr_t err = OOP_CALL(__printer, open);
    LOG_ERROR("open printer = %d", err);
    if (err != PRNT_ERR_OK) {
        return err;
    }
    PrinterStatus_t st = OOP_CALL(__printer, getStatus);
    if (st == PRNT_STAT_NO_PAPER) return PRNT_ERR_NO_PAPER;
    if (st == PRNT_STAT_BUSY || st == PRNT_STAT_OVER_HEAT) {
        // delay
        OOP_CALL(__printer, close);
        if (reopened) {
            return st == PRNT_STAT_BUSY ? PRNT_ERR_TIME_OUT : PRNT_STAT_OVER_HEAT;
        }
        reopened = true;
        goto reopen;
    }
    OOP_CALL(__printer, setGray, PRNT_GRAY_LVL_LOW);
    err = OOP_CALL(__printer, printBmp, src, width, height);
    OOP_CALL(__printer, close);
    return err;
}

OOP_CTOR(Printer) {
    __printer->print = print;
}

Printer *printer() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce printer is undefined. Make sure correct device is chosen and its printer driver is developed.
#endif
    );
    return __printer;
}