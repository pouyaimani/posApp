#include "msg.h"
#include "dev/dev.h"

static Parser *__parser;
static Packer *__packer;
static char *msgBuffer;

#if MSG_STANDARD == ISO8583

#include "iso8583/msg_iso8583.h"

static Iso8583Parser *__iso8583parser;
static Iso8583Packer *__iso8583packer;
static void constructIso8583() {
    CALL_ONCE(
        __iso8583parser = GET_MEM(sizeof(Iso8583Parser));
        __parser = (Iso8583Parser*)__iso8583parser;
        OOP_CALL_CTOR(Parser, __parser);
        OOP_CALL_CTOR(Iso8583Parser, __iso8583parser);

        __iso8583packer = GET_MEM(sizeof(Iso8583Packer));
        __packer = (Iso8583Packer*)__iso8583packer;
        OOP_CALL_CTOR(Packer, __packer);
        OOP_CALL_CTOR(Iso8583Packer, __iso8583packer);
    );
}

#endif

static void initMsg() {
    CALL_ONCE(
        msgBuffer = (char*)GET_MEM(MSG_BUFFER_SIZE);
    );
}

OOP_CTOR(Parser) {
    self->vtable.parse = NULL;
    self->buffer = msgBuffer;
};

Parser *parser() {
    CALL_ONCE(
        initMsg();
        constructIso8583(); 
    );
    return __parser;
}

OOP_CTOR(Packer) {
    self->vtable.pack = NULL;
    self->buffer = msgBuffer;
};

Packer *packer() {
    CALL_ONCE(
        initMsg();
        constructIso8583(); 
    );
    return __packer;
}