#include "pnaTmsMd5.h"

#include <string.h>

/* RFC 1321 MD5, used to match the legacy server's 16-byte digest field.
 * It does not authenticate an update. Do not rely on it to install firmware. */
static uint32_t rol32(uint32_t x, unsigned n) {
    return (x << n) | (x >> (32u - n));
}

static uint32_t load32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void store32(uint8_t* p, uint32_t x) {
    p[0] = (uint8_t)x;
    p[1] = (uint8_t)(x >> 8);
    p[2] = (uint8_t)(x >> 16);
    p[3] = (uint8_t)(x >> 24);
}

static void compress(PnaTmsMd5* ctx, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,
        0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
        0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,
        0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
        0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,
        0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
        0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,
        0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
        0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,
        0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
        0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x04881d05u,
        0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,
        0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,
        0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,
        0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,
        0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u
    };
    static const uint8_t s[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
    };
    uint32_t m[16];
    for (unsigned i = 0; i < 16; ++i)
        m[i] = load32(block + 4u * i);
    uint32_t a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3];
    for (unsigned i = 0; i < 64; ++i) {
        uint32_t f;
        unsigned g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5u * i + 1u) % 16u;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3u * i + 5u) % 16u;
        } else {
            f = c ^ (b | ~d);
            g = (7u * i) % 16u;
        }
        uint32_t oldD = d;
        d = c;
        c = b;
        b += rol32(a + f + k[i] + m[g], s[i]);
        a = oldD;
    }
    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
}

void pnaTmsMd5Init(PnaTmsMd5* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->h[0] = 0x67452301u;
    ctx->h[1] = 0xefcdab89u;
    ctx->h[2] = 0x98badcfeu;
    ctx->h[3] = 0x10325476u;
}

void pnaTmsMd5Update(PnaTmsMd5* ctx, const void* data, size_t length) {
    if (!ctx || (!data && length)) return;
    const uint8_t* bytes = (const uint8_t*)data;
    ctx->byteCount += (uint64_t)length;
    while (length) {
        size_t toCopy = 64u - ctx->pendingCount;
        if (toCopy > length) toCopy = length;
        memcpy(ctx->pending + ctx->pendingCount, bytes, toCopy);
        ctx->pendingCount += toCopy;
        bytes += toCopy;
        length -= toCopy;
        if (ctx->pendingCount == 64u) {
            compress(ctx, ctx->pending);
            ctx->pendingCount = 0u;
        }
    }
}

void pnaTmsMd5Final(PnaTmsMd5* ctx, uint8_t output[16]) {
    if (!ctx || !output) return;
    const uint64_t bitCount = ctx->byteCount * UINT64_C(8);
    static const uint8_t pad[64] = {0x80};
    const size_t padLen = ctx->pendingCount < 56u
                         ? 56u - ctx->pendingCount
                         : 120u - ctx->pendingCount;
    pnaTmsMd5Update(ctx, pad, padLen);
    uint8_t bitLen[8];
    store32(bitLen, (uint32_t)bitCount);
    store32(bitLen + 4, (uint32_t)(bitCount >> 32));
    pnaTmsMd5Update(ctx, bitLen, sizeof(bitLen));
    for (unsigned i = 0; i < 4; ++i) store32(output + 4u * i, ctx->h[i]);
    memset(ctx, 0, sizeof(*ctx));
}
