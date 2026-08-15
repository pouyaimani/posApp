#ifndef ISO_HELPER_H_
#define ISO_HELPER_H_

#include "iso8583.h"
#include "txn.h"
#include "error.h"

typedef int8_t (*FeildSetter)(TxnData* txn);
typedef int8_t (*FeildGetter)(TxnData* txn);

typedef struct {
    uint8_t     feild;
    FeildSetter set;
    FeildGetter get;
} IsoFeildsFunc;

void decodeMerchantDesc(char* buffer, TxnData* txn);

Error_t setMti(uint16_t mti);

extern const IsoFeildsFunc isoHelper[ISO_MAX_FIELDS];

#endif