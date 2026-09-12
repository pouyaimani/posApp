#include "printer.h"
#include <stdbool.h>
#include "logger.h"
#include "sys/sys.h"
#include "error.h"

Printer* __printer;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/printer_t3Rtos.h"

static void constructT3Rtos() {
    static PrinterT3Rtos obj;
    __printer = (Printer*)&obj;
    OOP_CALL_CTOR(Printer, __printer);
    OOP_CALL_CTOR(PrinterT3Rtos, &obj);
}

#endif

static PrinterErr_t init() {
    int reopened = false;
reopen:
    PrinterErr_t err = OOP_CALL(__printer, open);
    RETURN_VALUE_IF_NOT(err, PRNT_ERR_OK, ;, err);
    PrinterStatus_t st = OOP_CALL(__printer, getStatus);
    RETURN_VALUE_IF(st, PRNT_STAT_NO_PAPER, ;, PRNT_ERR_NO_PAPER);
    if (st == PRNT_STAT_BUSY || st == PRNT_STAT_OVER_HEAT) {
        // delay
        OOP_CALL(__printer, close);
        if (reopened) {
            return st == PRNT_STAT_BUSY ? PRNT_ERR_TIME_OUT
                                        : PRNT_STAT_OVER_HEAT;
        }
        reopened = true;
        goto reopen;
    }
    OOP_CALL(__printer, init);
    err = OOP_CALL(__printer, setGray, PRNT_GRAY_LVL_LOW);
    return err;
}

static PrinterErr_t finish() { return OOP_CALL(__printer, close); }

static PrinterErr_t print(uint8_t* src, uint16_t width, uint16_t height) {
    return OOP_CALL(__printer, printBmp, src, width, height);
}

OOP_CTOR(Printer) {
    __printer->print  = print;
    __printer->init   = init;
    __printer->finish = finish;
}

Printer* printer() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        constructT3Rtos();
#else
#error Deivce printer is undefined. Make sure correct device is chosen and its printer driver is developed.
#endif
    );
    return __printer;
}