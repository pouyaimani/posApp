#ifndef ARITH_H_
#define ARITH_H_

#include <stdint.h>

int libCrc16(const unsigned char *pucSrc, unsigned int siSrcLen);

int32_t bcdToAsc(uint8_t *pasDest, const uint8_t *pbcSrc, int32_t siBcdSrclen);

#endif