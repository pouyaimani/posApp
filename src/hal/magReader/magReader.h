#ifndef MAGREADER_H_
#define MAGREADER_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "event.h"

typedef enum MagReaderErr_t {
    MAG_ERR_SWIPED,
    MAG_ERR_OPEN_FAILED,
    MAG_ERR_NO_SWIPE,
    MAG_ERR_DATA_ERR,
    MAG_ERR_TIME_OUT,
    MAG_ERR_OTHER

} MagReaderErr_t;

OOP_DECLARE_CLASS(MagReader)

OOP_VTABLE(MagReader) {
    OOP_IMETHOD(void, MagReader, init);
    OOP_IMETHOD(void, MagReader, read);
    OOP_IMETHOD(bool, MagReader, isSwiped);
    OOP_IMETHOD(TrackData_t, MagReader, getTrack1);
    OOP_IMETHOD(TrackData_t, MagReader, getTrack2);
    OOP_IMETHOD(TrackData_t, MagReader, getTrack3);
};

OOP_CLASS(MagReader) {
    OOP_IMPLEMENTS(MagReader);
    OOP_METHOD(void, ioRead);
    bool swiped;
    MagReaderData_t data;
    MagReaderErr_t error;
};

OOP_CTOR(MagReader);

MagReader *getMagReader(void);

#define MAG_INIT() OOP_CALL(getMagReader(), init)

#endif