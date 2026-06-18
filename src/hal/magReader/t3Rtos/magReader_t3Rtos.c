#ifdef DEVICE_TRENDITT3RTOS

#include "magReader_t3Rtos.h"
#include "posplatform.h"
#include "sdkMag.h"
#include "logger.h"
#include "sdkemvapp.h"
#include "sdkUtils.h"

static SDK_ICC_PARAM    param;
static SDK_ICC_CARDDATA data;

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

bool isLegalTrack2(s8* track2) {
    u32 track2Len = 0;

    track2Len = track2 == NULL ? 0 : strlen(track2);
    if ((track2Len > 0 && track2Len <= 12 * 2) || (track2Len > 19 * 2) ||
        (track2Len == 0)) {
        return false;
    }
    return true;
}

static s32 libTransAdjustTrack(s8* track1, s8* track2, s8* track3) {
    u32 track1Len = 0, track2Len = 0, track3Len = 0;

    track1Len = track1 == NULL ? 0 : strlen(track1);
    track2Len = track2 == NULL ? 0 : strlen(track2);
    track3Len = track3 == NULL ? 0 : strlen(track3);

    if (!isLegalTrack2(track2)) {
        return SDK_MAG_ERR_INPUT;
    }

    if (track1Len > 76)
        track1[76] = '\0';
    if (track2Len > 19 * 2)
        track2[38] = '\0';
    if (track3Len > 52 * 2)
        track3[104] = '\0';

    return SDK_OK;
}

static void read(MagReader* mag) {
    int ret = sdkMagReadData(&param, &data);
    if (ret == SDK_OK) {
        ret =
            libTransAdjustTrack((s8*)data.asTrack1Data, (s8*)data.asTrack2Data,
                                (s8*)data.asTrack3Data);
        if (ret != SDK_OK) {
            mag->error = MAG_ERR_DATA_ERR;
            return;
        }
    } else {
        mag->error = MAG_ERR_NO_SWIPE;
        return;
    }
    mag->error = MAG_ERR_SWIPED;
}

OOP_CTOR(MagReaderT3Rtos) {
    self->base.vtable.init      = init;
    self->base.vtable.read      = read;
    self->base.data.track1.data = data.asTrack1Data;
    self->base.data.track2.data = data.asTrack2Data;
    self->base.data.track3.data = data.asTrack3Data;
}

#endif
