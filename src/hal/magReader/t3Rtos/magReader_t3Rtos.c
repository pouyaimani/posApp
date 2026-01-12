#ifdef DEVICE_TRENDITT3RTOS

#include "magReader_t3Rtos.h"
#include "posplatform.h"
#include "sdkMag.h"
#include "logger.h"

static SDK_MAG_CARD_DATA magData;

static MagReaderErr_t translateSdkErr(int err) {
    MagReaderErr_t magErr = MAG_ERR_NO_SWIPE;
    switch (err) {
    case SDK_MAG_OK:
        magErr = MAG_ERR_SWIPED;
        break;
    case SDK_MAG_ERR_INPUT:
        magErr = MAG_ERR_DATA_ERR;
        break;
    case SDK_MAG_ERR_BASE:
        magErr = MAG_ERR_OTHER;
        break;
    case SDK_MAG_ERR_TIMEOUT:
        magErr = MAG_ERR_TIME_OUT;
        break;
    default:
        break;
    }
    return magErr;
}

static void init(MagReader* mag) {
    mag->error = translateSdkErr(sdkMagOpen());
    sdkMagClear();
}

static void read(MagReader* mag) {
    int magErr = sdkMagRead(&magData);
    mag->error = translateSdkErr(magErr);
    if (mag->data.track2.len != 0) {
        LOG_TRACE("mag track 2 = %s", magData.track2Data);
        mag->data.track1.len = magData.track1Len;
        mag->data.track1.data = magData.track1Data;
        mag->data.track2.len = magData.track2Len;
        mag->data.track2.data = magData.track2Data;
        mag->data.track3.len = magData.track3Len;
        mag->data.track3.data = magData.track3Data;
    }
    sdkMagClear();
}

OOP_CTOR(MagReaderT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.read = read;
    self->base.data.track1.data = magData.track1Data;
    self->base.data.track2.data = magData.track2Data;
    self->base.data.track3.data = magData.track3Data;
}

#endif
