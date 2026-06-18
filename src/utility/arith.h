#ifndef ARITH_H_
#define ARITH_H_

#include <stdint.h>

int libCrc16(const unsigned char* pucSrc, unsigned int siSrcLen);

int32_t bcdToAsc(uint8_t* pasDest, const uint8_t* pbcSrc, int32_t siBcdSrclen);

int32_t ascToBcd(uint8_t* pbcDest, const char* pasSrc, uint32_t siSrclen);

int32_t hexToU32(uint32_t* puiDest, const uint8_t* pheSrc, uint32_t siHexLen);

void intToBytes(uint32_t src, uint8_t* bytes, uint32_t bytesLen);

int32_t u32ToHex(uint8_t* pheDest, uint32_t const uiSrc, uint32_t siHexLen);

uint8_t toBcd(int value);

#endif