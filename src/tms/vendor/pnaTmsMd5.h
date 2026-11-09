#ifndef PNA_TMS_MD5_H_
#define PNA_TMS_MD5_H_

#include <stddef.h>
#include <stdint.h>

/* Legacy TMS compatibility only. MD5 is NOT an authenticity mechanism. */
typedef struct {
    uint32_t h[4];
    uint64_t byteCount;
    uint8_t pending[64];
    size_t pendingCount;
} PnaTmsMd5;

void pnaTmsMd5Init(PnaTmsMd5* ctx);
void pnaTmsMd5Update(PnaTmsMd5* ctx, const void* data, size_t length);
void pnaTmsMd5Final(PnaTmsMd5* ctx, uint8_t output[16]);

#endif
