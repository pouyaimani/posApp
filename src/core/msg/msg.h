#ifndef MSG_H_
#define MSG_H_

#include "oop.h"

// Message Standards
#define ISO8583 1


#define MSG_STANDARD ISO8583
#define MSG_BUFFER_SIZE 1000


/**********************Parser********************/
OOP_DECLARE_CLASS(Parser);

OOP_VTABLE(Parser) {
    OOP_IMETHOD(void, Parser, parse, char *);
};

OOP_CLASS(Parser) {
    OOP_IMPLEMENTS(Parser);
    char *buffer;
};

OOP_CTOR(Parser);

Parser *parser();


/**********************Packer********************/
OOP_DECLARE_CLASS(Packer);

OOP_VTABLE(Packer) {
    OOP_IMETHOD(void, Packer, pack, char *);
};

OOP_CLASS(Packer) {
    OOP_IMPLEMENTS(Packer);
    char *buffer;
};

OOP_CTOR(Packer);

Packer *packer();

#endif