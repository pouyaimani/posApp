#include "printer.h"
#include <stdbool.h>
#include "logger.h"
#include "dev/dev.h"

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

inline unsigned char parseRgb(unsigned char *v, uint16_t colorDepth) {
    uint32_t value = (colorDepth == 16)
                   ? *(const uint16_t*)v
                   : *(const uint32_t*)v;
    return (value == 0);
}

static void rgbToBitmap(unsigned char *src, uint32_t size, uint16_t colorDepth, unsigned char *dst) 
{
    uint8_t it = colorDepth * 0.125; // 0.125 = 1 / 8
    for (int i = 0; i < size; i++) {
        dst[i] = parseRgb(src, colorDepth);
        src += it;
    }
}

static PrinterErr_t print(uint8_t *src, uint16_t width, uint16_t height, uint16_t colorDepth) {
    int reopened = false;
reopen:
    PrinterErr_t err = OOP_CALL(__printer, open);
    LOG_ERROR("open printer = %d", err);
    if (err != PRNT_ERR_OK) {
        return err;
    }
    PrinterStatus_t st = OOP_CALL(__printer, getStatus);
    LOG_ERROR("printer status = %d", st);
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
    // rgbToBitmap(src, width * height , colorDepth, buf);
    LOG_ERROR("width = %d, height = %d", width, height);
    err = OOP_CALL(__printer, printBmp, src, width, height);
    OOP_CALL(__printer, close);
    return err;
}

OOP_CTOR(Printer) {
    __printer->print = print;
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