#ifndef PRINTER_H_
#define PRINTER_H_

#include "oop.h"

OOP_DECLARE_CLASS(Printer)

OOP_VTABLE(Printer) {
    OOP_IMETHOD(void, Printer, init);
};

OOP_CLASS(Printer) {
    OOP_IMPLEMENTS(Printer);
    const char *name;
};

OOP_CTOR(Printer, const char* name);

Printer *getPrinter(void);

#endif