#ifndef ISO_BUILDER_H_
#define ISO_BUILDER_H_

#include "iso/iso8583.h"
#include "common.h"
#include "byteArray.h"
#include "txn.h"

Error_t    isoBuild(Mti_t mti, PrCode_t prcode, TxnCore* txn, ByteArray* buf);
RespCode_t isoParse(Mti_t mti, PrCode_t prcode, ByteArray* buf);

typedef int8_t (*TxnIsoBuilder)(TxnCore* txn, ByteArray*);
typedef int8_t (*TxnIsoParser)(ByteArray*);

typedef struct {
    Mti_t         mti;
    PrCode_t      prcode;
    TxnIsoBuilder builder;
    TxnIsoParser  parser;
} IsoTransaction;

#endif