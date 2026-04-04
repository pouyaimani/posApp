#ifndef PRINTER_H_
#define PRINTER_H_

#include "oop.h"
#include <stdint.h>

typedef enum PrinterGrayLevel_t {
    PRNT_GRAY_LVL_LOW,
    PRNT_GRAY_LVL_MEDIUM,
    PRNT_GRAY_LVL_HIGH
} PrinterGrayLevel_t;

typedef enum PrinterErr_t {
    PRNT_ERR_OK,
    PRNT_ERR_NOK,
    PRNT_ERR_INPUT,
    PRNT_ERR_OVER_HEAT,
    PRNT_ERR_NO_PAPER,
    PRNT_ERR_TIME_OUT
} PrinterErr_t;

typedef enum PrinterStatus_t {
    PRNT_STAT_READY,
    PRNT_STAT_BUSY,
    PRNT_STAT_NO_PAPER,
    PRNT_STAT_OVER_HEAT,
    PRNT_STAT_OTHER
} PrinterStatus_t;

OOP_DECLARE_CLASS(Printer)

OOP_VTABLE(Printer) {
    OOP_IMETHOD(void, Printer, init);
    OOP_IMETHOD(PrinterErr_t, Printer, open);
    OOP_IMETHOD(PrinterErr_t, Printer, close);
    OOP_IMETHOD(PrinterErr_t, Printer, setGray, PrinterGrayLevel_t);
    OOP_IMETHOD(PrinterGrayLevel_t, Printer, getGray);
    OOP_IMETHOD(PrinterStatus_t, Printer, getStatus);
    OOP_IMETHOD(PrinterErr_t, Printer, printBmp, uint8_t *, uint16_t, uint16_t);
};

OOP_CLASS(Printer) {
    OOP_IMPLEMENTS(Printer);
    OOP_METHOD(PrinterErr_t, print, uint8_t *, uint16_t, uint16_t);
    PrinterStatus_t status;
    PrinterErr_t error;
};

OOP_CTOR(Printer);

Printer *getPrinter(void);

#define PRINTER_INIT() OOP_CALL(getPrinter(), init)

#endif