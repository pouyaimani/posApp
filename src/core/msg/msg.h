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
    ISO_MSG_STR = 0,
    ISO_MSG_BYTE
} IsoMsgType_t;

typedef enum {
    MSG_ERR_OK = 0,
    MSG_ERR_PARSE_FAILED,
} IsoMsgErr_t;

#define ISO_MSG_TYPE (IsoMsgType_t[MSG_FIELDS_CONUT]) \
{\
    /*field 0*/  ISO_MSG_BYTE,\
    /*field 1*/  ISO_MSG_STR,\
    /*field 2*/  ISO_MSG_STR,\
    /*field 3*/  ISO_MSG_STR,\
    /*field 4*/  ISO_MSG_STR,\
    /*field 5*/  ISO_MSG_STR,\
    /*field 6*/  ISO_MSG_STR,\
    /*field 7*/  ISO_MSG_STR,\
    /*field 8*/  ISO_MSG_STR,\
    /*field 9*/  ISO_MSG_STR,\
    /*field 10*/ ISO_MSG_STR,\
    /*field 11*/ ISO_MSG_STR,\
    /*field 12*/ ISO_MSG_STR,\
    /*field 13*/ ISO_MSG_STR,\
    /*field 14*/ ISO_MSG_STR,\
    /*field 15*/ ISO_MSG_STR,\
    /*field 16*/ ISO_MSG_STR,\
    /*field 17*/ ISO_MSG_STR,\
    /*field 18*/ ISO_MSG_STR,\
    /*field 19*/ ISO_MSG_STR,\
    /*field 20*/ ISO_MSG_STR,\
    /*field 21*/ ISO_MSG_STR,\
    /*field 22*/ ISO_MSG_STR,\
    /*field 23*/ ISO_MSG_STR,\
    /*field 24*/ ISO_MSG_STR,\
    /*field 25*/ ISO_MSG_STR,\
    /*field 26*/ ISO_MSG_STR,\
    /*field 27*/ ISO_MSG_STR,\
    /*field 28*/ ISO_MSG_STR,\
    /*field 29*/ ISO_MSG_STR,\
    /*field 30*/ ISO_MSG_STR,\
    /*field 31*/ ISO_MSG_STR,\
    /*field 32*/ ISO_MSG_STR,\
    /*field 33*/ ISO_MSG_STR,\
    /*field 34*/ ISO_MSG_STR,\
    /*field 35*/ ISO_MSG_STR,\
    /*field 36*/ ISO_MSG_STR,\
    /*field 37*/ ISO_MSG_STR,\
    /*field 38*/ ISO_MSG_STR,\
    /*field 39*/ ISO_MSG_STR,\
    /*field 40*/ ISO_MSG_STR,\
    /*field 41*/ ISO_MSG_STR,\
    /*field 42*/ ISO_MSG_STR,\
    /*field 43*/ ISO_MSG_STR,\
    /*field 44*/ ISO_MSG_STR,\
    /*field 45*/ ISO_MSG_STR,\
    /*field 46*/ ISO_MSG_STR,\
    /*field 47*/ ISO_MSG_STR,\
    /*field 48*/ ISO_MSG_STR,\
    /*field 49*/ ISO_MSG_STR,\
    /*field 50*/ ISO_MSG_STR,\
    /*field 51*/ ISO_MSG_STR,\
    /*field 52*/ ISO_MSG_BYTE,\
    /*field 53*/ ISO_MSG_STR,\
    /*field 54*/ ISO_MSG_STR,\
    /*field 55*/ ISO_MSG_STR,\
    /*field 56*/ ISO_MSG_STR,\
    /*field 57*/ ISO_MSG_STR,\
    /*field 58*/ ISO_MSG_BYTE,\
    /*field 59*/ ISO_MSG_STR,\
    /*field 60*/ ISO_MSG_STR,\
    /*field 61*/ ISO_MSG_STR,\
    /*field 62*/ ISO_MSG_STR,\
    /*field 63*/ ISO_MSG_BYTE,\
    /*field 64*/ ISO_MSG_BYTE,\
}\

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
    OOP_IMETHOD(IsoMsgErr_t, Parser, reset);
};

OOP_CLASS(Parser) {
    OOP_IMPLEMENTS(Parser);
    char *buffer;
    DataElement *element;
};

OOP_CTOR(Parser);

Parser *parser();

/**********************Packer********************/

OOP_DECLARE_CLASS(Packer);

OOP_VTABLE(Packer) {
    OOP_IMETHOD(IsoMsgErr_t, Packer, pack, char *);
    OOP_IMETHOD(IsoMsgErr_t, Packer, reset);
};

OOP_CLASS(Packer) {
    OOP_IMPLEMENTS(Packer);
    char *buffer;
    DataElement *element;
};

OOP_CTOR(Packer);

Packer *packer();

#endif