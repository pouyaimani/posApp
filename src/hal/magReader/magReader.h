#ifndef MAGREADER_H_
#define MAGREADER_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "event.h"
#include "error.h"

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
    OOP_METHOD(void, getPan, char* pan, size_t len);
    bool            swiped;
    MagReaderData_t data;
    MagReaderErr_t  error;
};

OOP_CTOR(MagReader);

MagReader* magreader(void);

#define MAG_INIT() OOP_CALL(magreader(), init)

#endif