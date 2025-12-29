#ifndef PRINTER_T3RTOS_H_
#define PRINTER_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../printer.h"

OOP_CLASS(PrinterT3Rtos) {
    OOP_EXTENDS(Printer);
};

OOP_CTOR(PrinterT3Rtos, const char* name);

#endif

#endif