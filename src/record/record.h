#ifndef RECORD_H_
#define RECORD_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"

typedef struct {
    uint8_t status;
    uint8_t type;

    uint8_t voucher[3];
    uint8_t amount[6];

    uint8_t time[6];

    uint8_t reserve[8];

    char pan[24];
    char refId[24];

    uint16_t prCode;
    uint16_t rrn;
} TxnRecord;

OOP_CLASS(Record) {
    OOP_METHOD(void, init);
    OOP_METHOD(void, add, TxnRecord *);
    OOP_METHOD(void, readByIdx, uint32_t idx, TxnRecord *);
    OOP_METHOD(int32_t, getLastIdx);
};

OOP_CTOR(Record);

Record *record(void);

#endif