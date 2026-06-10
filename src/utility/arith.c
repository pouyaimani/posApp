#include "arith.h"
#include <stdint.h>
#include <stddef.h>

static const unsigned short CRCTAB[] =
    {
        0x0000, 0x8005, 0x800f, 0x000a, 0x801b, 0x001e, 0x0014, 0x8011,
        0x8033, 0x0036, 0x003c, 0x8039, 0x0028, 0x802d, 0x8027, 0x0022,
        0x8063, 0x0066, 0x006c, 0x8069, 0x0078, 0x807d, 0x8077, 0x0072,
        0x0050, 0x8055, 0x805f, 0x005a, 0x804b, 0x004e, 0x0044, 0x8041,
        0x80c3, 0x00c6, 0x00cc, 0x80c9, 0x00d8, 0x80dd, 0x80d7, 0x00d2,
        0x00f0, 0x80f5, 0x80ff, 0x00fa, 0x80eb, 0x00ee, 0x00e4, 0x80e1,
        0x00a0, 0x80a5, 0x80af, 0x00aa, 0x80bb, 0x00be, 0x00b4, 0x80b1,
        0x8093, 0x0096, 0x009c, 0x8099, 0x0088, 0x808d, 0x8087, 0x0082,
        0x8183, 0x0186, 0x018c, 0x8189, 0x0198, 0x819d, 0x8197, 0x0192,
        0x01b0, 0x81b5, 0x81bf, 0x01ba, 0x81ab, 0x01ae, 0x01a4, 0x81a1,
        0x01e0, 0x81e5, 0x81ef, 0x01ea, 0x81fb, 0x01fe, 0x01f4, 0x81f1,
        0x81d3, 0x01d6, 0x01dc, 0x81d9, 0x01c8, 0x81cd, 0x81c7, 0x01c2,
        0x0140, 0x8145, 0x814f, 0x014a, 0x815b, 0x015e, 0x0154, 0x8151,
        0x8173, 0x0176, 0x017c, 0x8179, 0x0168, 0x816d, 0x8167, 0x0162,
        0x8123, 0x0126, 0x012c, 0x8129, 0x0138, 0x813d, 0x8137, 0x0132,
        0x0110, 0x8115, 0x811f, 0x011a, 0x810b, 0x010e, 0x0104, 0x8101,
        0x8303, 0x0306, 0x030c, 0x8309, 0x0318, 0x831d, 0x8317, 0x0312,
        0x0330, 0x8335, 0x833f, 0x033a, 0x832b, 0x032e, 0x0324, 0x8321,
        0x0360, 0x8365, 0x836f, 0x036a, 0x837b, 0x037e, 0x0374, 0x8371,
        0x8353, 0x0356, 0x035c, 0x8359, 0x0348, 0x834d, 0x8347, 0x0342,
        0x03c0, 0x83c5, 0x83cf, 0x03ca, 0x83db, 0x03de, 0x03d4, 0x83d1,
        0x83f3, 0x03f6, 0x03fc, 0x83f9, 0x03e8, 0x83ed, 0x83e7, 0x03e2,
        0x83a3, 0x03a6, 0x03ac, 0x83a9, 0x03b8, 0x83bd, 0x83b7, 0x03b2,
        0x0390, 0x8395, 0x839f, 0x039a, 0x838b, 0x038e, 0x0384, 0x8381,
        0x0280, 0x8285, 0x828f, 0x028a, 0x829b, 0x029e, 0x0294, 0x8291,
        0x82b3, 0x02b6, 0x02bc, 0x82b9, 0x02a8, 0x82ad, 0x82a7, 0x02a2,
        0x82e3, 0x02e6, 0x02ec, 0x82e9, 0x02f8, 0x82fd, 0x82f7, 0x02f2,
        0x02d0, 0x82d5, 0x82df, 0x02da, 0x82cb, 0x02ce, 0x02c4, 0x82c1,
        0x8243, 0x0246, 0x024c, 0x8249, 0x0258, 0x825d, 0x8257, 0x0252,
        0x0270, 0x8275, 0x827f, 0x027a, 0x826b, 0x026e, 0x0264, 0x8261,
        0x0220, 0x8225, 0x822f, 0x022a, 0x823b, 0x023e, 0x0234, 0x8231,
        0x8213, 0x0216, 0x021c, 0x8219, 0x0208, 0x820d, 0x8207, 0x0202};

int libCrc16(const unsigned char *pucSrc, unsigned int siSrcLen)
{
    unsigned short crcData = 0;

    if ((NULL == pucSrc) || (siSrcLen <= 0))
    {

        return -1;
    }

    while (siSrcLen > 0)
    {
        crcData = (crcData << 8) ^ CRCTAB[(crcData >> 8) ^ (*pucSrc)];
        siSrcLen--;
        pucSrc++;
    }

    return crcData;
}

int32_t bcdToAsc(uint8_t *pasDest, const uint8_t *pbcSrc, int32_t siBcdSrclen)
{
    int32_t i = 0;

    if ((NULL == pbcSrc) || (NULL == pasDest) || (siBcdSrclen < 0))
    {
        return (-4);
    }

    for (i = 0; i < siBcdSrclen; i++)
    {
        // 高Nibble转换
        if (((*(pbcSrc + i) & 0xF0) >> 4) <= 9)
        {
            *(pasDest + 2 * i) = ((*(pbcSrc + i) >> 4) & 0X0F) + 0x30;
        }
        else
        {
            *(pasDest + 2 * i) = ((*(pbcSrc + i) >> 4) & 0x0F) + 0x37; // 大写A~F
        }

        // 低Nibble转换
        if ((*(pbcSrc + i) & 0x0F) <= 9)
        {
            *(pasDest + 2 * i + 1) = (*(pbcSrc + i) & 0x0F) + 0x30;
        }
        else
        {
            *(pasDest + 2 * i + 1) = (*(pbcSrc + i) & 0x0F) + 0x37; // 大写A~F
        }
    }

    return siBcdSrclen * 2;
}

int32_t ascToBcd(uint8_t *pbcDest, const char *pasSrc, uint32_t siSrclen)
{
    uint32_t i = 0;
    uint8_t high = 0, low = 0;

    if ((NULL == pasSrc) || (NULL == pbcDest) || (siSrclen < 0))
    {

        return -1;
    }

    for (i = 0; i < siSrclen; i++)
    {
        if ((pasSrc[i] < '0') || ((pasSrc[i] > '9') && (pasSrc[i] < 'A')) || ((pasSrc[i] > 'F') && (pasSrc[i] < 'a')) || (pasSrc[i] > 'f'))
        {
            return -1;
        }
    }

    for (i = 0; i < siSrclen; i++)
    {

        if ((*(pasSrc + i) >= 0x61) && (*(pasSrc + i) <= 0x66))
        {
            high = (uint8_t)(*(pasSrc + i) - 0x57);
        }
        else if ((*(pasSrc + i) >= 0x41) && (*(pasSrc + i) <= 0x46))
        {
            high = (uint8_t)(*(pasSrc + i) - 0x37);
        }
        else if ((*(pasSrc + i) >= 0x30) && (*(pasSrc + i) <= 0x39))
        {
            high = (uint8_t)(*(pasSrc + i) - 0x30);
        }
        else
        {
            high = 0x00;
        }

        i++;

        if (i < siSrclen)
        {
            if ((*(pasSrc + i) >= 0x61) && (*(pasSrc + i) <= 0x66))
            {
                low = (uint8_t)(*(pasSrc + i) - 0x57);
            }
            else if ((*(pasSrc + i) >= 0x41) && (*(pasSrc + i) <= 0x46))
            {
                low = (uint8_t)(*(pasSrc + i) - 0x37);
            }
            else if ((*(pasSrc + i) >= 0x30) && (*(pasSrc + i) <= 0x39))
            {
                low = (uint8_t)(*(pasSrc + i) - 0x30);
            }
            else
            {
                low = 0x00;
            }
        }
        else
        {
            i--;
            low = 0x00;
        }
        *(pbcDest + i / 2) = (high << 4) | low;
    }

    return (siSrclen + 1) / 2;
}

int32_t hexToU32(uint32_t *puiDest, const uint8_t *pheSrc, uint32_t siHexLen)
{
    uint32_t i = 0;

    if ((NULL == pheSrc) || (NULL == puiDest) || (siHexLen < 0))
    {

        return -1;
    }
    *puiDest = 0;

    for (i = 0; i < siHexLen; i++)
    {
        *puiDest *= 256;
        *puiDest += pheSrc[i];
    }

    return 0;
}

void intToBytes(uint32_t src, uint8_t *bytes, uint32_t bytesLen)
{
    if (bytes == NULL || (bytesLen != 2 && bytesLen != 4))
    {
        return;
    }

    if (bytesLen == 2)
    {
        bytes[0] = ((src >> 8u) & 0xFFu);
        bytes[1] = (src & 0xFFu);
    }
    else
    {
        bytes[0] = ((src >> 24u) & 0xFFu);
        bytes[1] = ((src >> 16u) & 0xFFu);
        bytes[2] = ((src >> 8u) & 0xFFu);
        bytes[3] = (src & 0xFFu);
    }
}

int32_t u32ToHex(uint8_t *pheDest, uint32_t const uiSrc, uint32_t siHexLen)
{
    uint32_t i = 0;
    uint32_t num = uiSrc;

    if ((NULL == pheDest) || (siHexLen < 0))
    {
        return -1;
    }

    for (i = siHexLen; i > 0; i--)
    {
        *(pheDest + i - 1) = (uint8_t)(num % 256);
        num /= 256;
    }

    return siHexLen;
}

uint8_t toBcd(int value) {
    return (uint8_t)(((value / 10) << 4) |
                      (value % 10));
}