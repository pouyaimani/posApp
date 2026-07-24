#ifndef ISO_HANDLER_H_
#define ISO_HANDLER_H_

#include "iso/iso8583.h"
#include "common.h"
#include "byteArray.h"
#include "txn.h"

Error_t isoBuild(Mti_t mti, PrCode_t prcode, uint8_t* feild, uint16_t feildsCnt,
                 TxnData* txn, ByteArray* buf);
RespCode_t isoParse(Mti_t mti, PrCode_t prcode, TxnData* txn, ByteArray* buf);

typedef int8_t (*TxnIsoBuilder)(TxnData* txn, ByteArray*);
typedef int8_t (*TxnIsoParser)(TxnData* txn, ByteArray*);

typedef struct {
    Mti_t         mti;
    PrCode_t      prcode;
    TxnIsoBuilder builder;
    TxnIsoParser  parser;
} IsoTransaction;

#endif