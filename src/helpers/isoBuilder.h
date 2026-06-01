#ifndef ISO_BUILDER_H_
#define ISO_BUILDER_H_

#include "iso/iso8583.h"
#include "common.h"
#include "byteArray.h"
#include "txn.h"

Error_t isoBuild(MTI_t mti, ByteArray *buf);

typedef int8_t (*IsoBuilder)(
    ByteArray *);

typedef struct {
    MTI_t mti;
    IsoBuilder builder;
} IsoTemplate;

#endif