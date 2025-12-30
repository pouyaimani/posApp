#ifdef DEVICE_TRENDITT3RTOS

#include "printer_t3Rtos.h"
#include "posplatform.h"
#include "sdkPrint.h"

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
    case SDK_PRINT_ERR:
        status = PRNT_STAT_OTHER;
        break;
    default:
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
    default:
        break;
    }
    return error;
}

static void init(PrinterT3Rtos* dev) {
}

static PrinterStatus_t getStatus() {
    return translateSdkStatus(sdkPrintGetStatus());
}

static PrinterErr_t setGray(PrinterGrayLevel_t level) {
    PRINTER_GRAY_LEVEL lv = SDK_PRINT_GRAY_M;
    if (level == PRNT_GRAY_LVL_LOW) {
        lv = SDK_PRINT_GRAY_L;
    } else if (level == PRNT_GRAY_LVL_HIGH) {
        lv = SDK_PRINT_GRAY_H;
    }
    return translateSdkErr(sdkPrintSetGray(lv));
}

PrinterGrayLevel_t getGray() {
    PRINTER_GRAY_LEVEL g = sdkPrintGetGray();
    PrinterGrayLevel_t gray = g == SDK_PRINT_GRAY_M ? PRNT_GRAY_LVL_MEDIUM : PRNT_GRAY_LVL_LOW;
    gray = g == SDK_PRINT_GRAY_L ? PRNT_GRAY_LVL_LOW : PRNT_GRAY_LVL_HIGH;
    return gray;
}

static PrinterErr_t print(unsigned char *bmp, uint16_t width, uint16_t height) {
    PrintFormat format = {0};
    format.mAlign = PRINT_ALIGN_MIDDLE;
    format.mReverse = PRINT_REV_LINE_FILL;
    sdkPrintImage(&format, (const u8 *)bmp + 4, width, height);
}

void PrinterT3Rtos_ctor(PrinterT3Rtos* self) {
    self->base.vtable.init = init;
    self->base.vtable.getStatus = getStatus;
    self->base.vtable.setGray = setGray;
    self->base.vtable.getGray = getGray;
    self->base.vtable.print = print;
}

#endif