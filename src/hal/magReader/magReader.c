#include "magReader.h"
#include "sys/sys.h"
#include "eventloop.h"
#include "logger.h"
#include <stdint.h>

MagReader *__magReader;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/magReader_t3Rtos.h"

static void constructT3Rtos() {
    static MagReaderT3Rtos obj;
    __magReader = (MagReader *)&obj;
    OOP_CALL_CTOR(MagReader, __magReader);
    OOP_CALL_CTOR(MagReaderT3Rtos, &obj);
}

#endif

#ifndef MAG_READER_TRACK_1_SIZE
#error Please define mag reader track 1 size in magReader_<device>.h file (MAG_READER_TRACK_1_SIZE)
#endif

#ifndef MAG_READER_TRACK_2_SIZE
#error Please define mag reader track 2 size in magReader_<device>.h file (MAG_READER_TRACK_2_SIZE)
#endif

#ifndef MAG_READER_TRACK_3_SIZE
#error Please define mag reader track 3 size in magReader_<device>.h file (MAG_READER_TRACK_3_SIZE)
#endif

static bool isSwiped(MagReader *mag) {
    return mag->error == MAG_ERR_SWIPED;
}

static TrackData_t getTrack1(MagReader *mag) {
    return mag->data.track1;
}

static TrackData_t getTrack2(MagReader *mag) {
    return mag->data.track2;
}

static TrackData_t getTrack3(MagReader *mag) {
    return mag->data.track3;
}

static void readIo() {
    OOP_CALL(__magReader, read);
    if(isSwiped(__magReader)) {
        MagEvent *ev = (MagEvent*)createEvent(SM_EVENT_MAG);
        ev->data = &__magReader->data;
        DISPATCH_EVENT(ev);
    }
}

static int32_t getCardNoFormTrack(int8_t *track2, int8_t *cardNo, uint32_t cardNoMaxLen)
{
    bool isTrack2 = false;
    int8_t trackData[128] = {0};
    uint32_t i = 0;
    uint32_t len = 0;

    if (((track2 == NULL || strlen(track2) == 0)) || cardNo == NULL)
    {
        return -1;
    }

    memcpy(trackData, track2, strlen(track2) > 37 ? 37 : strlen(track2));

    len = strlen(trackData);
    for (i = 0; i < len; i++)
    {
        if (trackData[i] == 'D')
        {
            break;
        }
    }

    len = (uint8_t) (i > 19 ? 19 : i);
    trackData[len] = 0;

    snprintf(cardNo, cardNoMaxLen, "%s", trackData);
    return strlen(cardNo) > 0 ? 0 : -1;
}

static void getPan(char *pan, size_t len) {
    TrackData_t track2 = OOP_CALL(__magReader, getTrack2);
    getCardNoFormTrack(track2.data, pan, len);
}

OOP_CTOR(MagReader) {
    self->vtable.isSwiped = isSwiped;
    self->vtable.getTrack1 = getTrack1;
    self->vtable.getTrack2 = getTrack2;
    self->vtable.getTrack3 = getTrack3;
    self->ioRead = readIo;
    self->getPan = getPan;
}

MagReader *magreader() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce mag reader is undefined. Make sure correct device is chosen and its mag reader driver is developed.
#endif
    );
    return __magReader;
}