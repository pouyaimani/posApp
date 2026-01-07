#include "magReader.h"
#include "../dev/dev.h"

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

static void readIo(MagReader *mag) {
    OOP_CALL(mag, read);
    if(isSwiped(mag)) {
        MagEvent **ev = (MagEvent**)createEvent(SM_EVENT_MAG);
    }
}

OOP_CTOR(MagReader) {
    self->vtable.isSwiped = isSwiped;
    self->vtable.getTrack1 = getTrack1;
    self->vtable.getTrack2 = getTrack2;
    self->vtable.getTrack3 = getTrack3;
    self->vtable.readIo = readIo;
}

MagReader *getMagReader() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce mag reader is undefined. Make sure correct device is chosen and its mag reader driver is developed.
#endif
    );
    return __magReader;
}