#include "msg_iso8583.h"

#if MSG_STANDARD == ISO8583

static void parse(const char *data) {

}

OOP_CTOR(Iso8583Parser) {
    self->base.vtable.parse = parse;
}

static void pack(const char *data) {

}

OOP_CTOR(Iso8583Packer) {
    self->base.vtable.pack = pack;
}


#endif