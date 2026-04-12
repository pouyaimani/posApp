#ifndef RECORD_H_
#define RECORD_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"

typedef enum {
    REC_MATCH_INDEX,
    REC_MATCH_VOUCHER,
    REC_MATCH_TIME,
    REC_MATCH_TRANS_STATUS,
} RecordMatchType_t;

typedef struct {
    u8 mStatus;
    u8 mType;
    u8 mVoucher[3];
    u8 mAmount[6];
    u8 mTime[6];
    u8 mReserve[8];
} RecordIdx;

OOP_CLASS(TxnRecord) {
    RecordIdx idx;
    uint8_t pan[24];
    uint8_t refId[24];
    uint8_t date[8];
    uint8_t time[8];
    uint16_t prCode;
    uint16_t rrn;
};

OOP_DECLARE_CLASS(Record)

OOP_CLASS(Record) {
    OOP_METHOD(void, init);
    OOP_METHOD(void, add, TxnRecord *);
    OOP_METHOD(void, readByIdx, uint32_t idx, RecordIdx *, TxnRecord *);
    OOP_METHOD(int32_t, getLastIdx);
};

OOP_CTOR(Record);

Record *record(void);

#endif