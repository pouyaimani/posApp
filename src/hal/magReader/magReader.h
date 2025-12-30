#ifndef MAGREADER_H_
#define MAGREADER_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum MagReaderErr_t {
    MAG_ERR_SWIPED,
    MAG_ERR_OPEN_FAILED,
    MAG_ERR_NO_SWIPE,
    MAG_ERR_DATA_ERR,
    MAG_ERR_TIME_OUT,
    MAG_ERR_OTHER

} MagReaderErr_t;

typedef struct TrackData_t {
    uint16_t len;
    char *data;
} TrackData_t;

typedef struct MagReaderData_t {
    TrackData_t track1;
    TrackData_t track2;
    TrackData_t track3;
} MagReaderData_t;

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
    bool swiped;
    MagReaderData_t data;
    MagReaderErr_t error;
};

OOP_CTOR(MagReader);

MagReader *getMagReader(void);

#endif