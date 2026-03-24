#ifdef DEVICE_TRENDITT3RTOS

#include "printer_t3Rtos.h"
#include "posplatform.h"
#include "sdkPrint.h"
#include "logger.h"

PrinterStatus_t translateSdkStatus(PRINTER_TASK_STATUS st) {
    PrinterStatus_t status;
    switch (st) {
    case SDK_PRINT_FREE:
        status = PRNT_STAT_READY;
        break;
    case SDK_PRINT_PRINTING:
        status = PRNT_STAT_BUSY;
        break;
    case SDK_PRINT_NO_PAPER:
        status = PRNT_STAT_NO_PAPER;
        break;
    case SDK_PRINT_OVER_TEMP:
        status = PRNT_STAT_OVER_HEAT;
        break;
    default:
        status = PRNT_STAT_OTHER;
        break;
    }
    return status;
}

PrinterErr_t translateSdkErr(int err) {
    PrinterErr_t error = PRNT_ERR_NOK;
    switch (err) {
    case SDK_PRINT_OK:
        error = PRNT_ERR_OK;
        break;
    case SDK_PRINT_ERR_INPUT:
        error = PRNT_ERR_INPUT;
        break;
    case SDK_PRINT_ERR_TIMEOUT:
        error = PRNT_ERR_TIME_OUT;
        break;
    case SDK_PRINT_ERR_NO_PAPER:
        error = PRNT_ERR_NO_PAPER;
        break;
    case SDK_PRINT_ERR_OVER_HEAT:
        error = PRNT_ERR_OVER_HEAT;
        break;
    default:
        break;
    }
    return error;
}

static void init(Printer* dev) {
}

static PrinterErr_t open(Printer* dev) {
    return translateSdkErr(sdkPrintOpen());
}

static PrinterErr_t close(Printer* dev) {
    return translateSdkErr(sdkPrintClose());
}

static PrinterStatus_t getStatus(Printer* priter) {
    return translateSdkStatus(sdkPrintGetStatus());
}

static PrinterErr_t setGray(Printer* priter, PrinterGrayLevel_t level) {
    PRINTER_GRAY_LEVEL lv = SDK_PRINT_GRAY_M;
    if (level == PRNT_GRAY_LVL_LOW) {
        lv = SDK_PRINT_GRAY_L;
    } else if (level == PRNT_GRAY_LVL_HIGH) {
        lv = SDK_PRINT_GRAY_H;
    }
    return translateSdkErr(sdkPrintSetGray(lv));
}

PrinterGrayLevel_t getGray(Printer* priter) {
    PRINTER_GRAY_LEVEL g = sdkPrintGetGray();
    PrinterGrayLevel_t gray = g == SDK_PRINT_GRAY_M ? PRNT_GRAY_LVL_MEDIUM : PRNT_GRAY_LVL_LOW;
    gray = g == SDK_PRINT_GRAY_L ? PRNT_GRAY_LVL_LOW : PRNT_GRAY_LVL_HIGH;
    return gray;
}

static PrinterErr_t printBmp(Printer* priter, uint8_t *bmp, uint16_t width, uint16_t height) {
    PrintFormat format = {0};
    LOG_ERROR("width = %d, height = %d", width, height);
    int ret = sdkPrintImage(&format, 
        (const u8 *)bmp, 200, height);
    LOG_ERROR("sdkPrintImage ret = %d", ret);
    // return translateSdkErr(ret);
    return translateSdkErr(ret);
}

OOP_CTOR(PrinterT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.open = open;
    self->base.vtable.close = close;
    self->base.vtable.getStatus = getStatus;
    self->base.vtable.setGray = setGray;
    self->base.vtable.getGray = getGray;
    self->base.vtable.printBmp = printBmp;
}

#endif