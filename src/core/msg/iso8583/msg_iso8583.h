#ifndef MSG_ISO8583_H_
#define MSG_ISO8583_H_

#include "../msg.h"

#if MSG_STANDARD == ISO8583

OOP_CLASS(Iso8583Parser) {
    OOP_EXTENDS(Parser);
};

OOP_CTOR(Iso8583Parser);

OOP_CLASS(Iso8583Packer) {
    OOP_EXTENDS(Packer);
};

OOP_CTOR(Iso8583Packer);

#endif

#endif