#ifndef MSG_H_
#define MSG_H_

#include "oop.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>

// Message Standards
#define ISO8583 1

#define MSG_STANDARD ISO8583
#define MSG_BUFFER_SIZE 1000
#define MSG_FIELDS_CONUT 64
#define MSG_FIELD_SIZE 128

typedef enum {
    MSG_ERR_OK = 0,
    MSG_ERR_PARSE_FAILED,
} IsoMsgErr_t;

OOP_CLASS(DataElement) {
    uint8_t *data;
    uint16_t len;
    OOP_METHOD(bool, isFilled, int);
    OOP_METHOD(void, reset, int);
};

/**********************Parser********************/
OOP_DECLARE_CLASS(Parser);

OOP_VTABLE(Parser) {
    OOP_IMETHOD(IsoMsgErr_t, Parser, parse, char *);
};

OOP_CLASS(Parser) {
    OOP_IMPLEMENTS(Parser);
    OOP_METHOD(IsoMsgErr_t, reset);
    char *buffer;
    DataElement *element;
};

OOP_CTOR(Parser);

Parser *parser();

/**********************Packer********************/

OOP_DECLARE_CLASS(Packer);

OOP_VTABLE(Packer) {
    OOP_IMETHOD(IsoMsgErr_t, Packer, pack, char *);
    OOP_IMETHOD(void, Packer, setAmount, char *);
    OOP_IMETHOD(char *, Packer, getAmount);
};

OOP_CLASS(Packer) {
    OOP_IMPLEMENTS(Packer);
    OOP_METHOD(IsoMsgErr_t, reset);
    char *buffer;
    DataElement *element;
};

OOP_CTOR(Packer);

Packer *packer();

#endif