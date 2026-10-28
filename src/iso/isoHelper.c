#include "isoHelper.h"
#include "sys/sys.h"
#include "settings/settings.h"
#include "ped/ped.h"
#include "utility/ltv.h"
#include "utility/utility.h"
#include "utility/arith.h"
#include "magReader/magReader.h"
#include "emvMgr/emv_kahroba.h"
#include "txnMgr/magCardTxnCtx.h"
#include "version.h"

enum {
    TAG_MERCHANT_NAME      = 31,
    TAG_MERCHANT_PHONE     = 34,
    TAG_VOUCHER_SERIAL     = 40,
    TAG_VOUCHER_PIN        = 41,
    TAG_SWITCH_DATETIME    = 50,
    TAG_FORCE_TMS          = 94,
    TAG_MERCHANT_UNIQUE_ID = 98,
};

/**
 * @brief Convert EMV Track 2 (tag 57) from packed BCD to
 *        legacy-compatible ASCII representation.
 *
 * D -> '='
 * F -> 'F' (only permitted as final padding nibble)
 *
 * Example:
 *   12 34 56 D2 61 22 01 23 4F
 *   -> "123456=2612201234F"
 *
 * @note Never log the returned Track 2 contents.
 */
static bool emvTrack2ToString(const uint8_t* input, size_t inputLen,
                              char* output, size_t outputSize) {
    if (!input || !output || inputLen == 0u || outputSize == 0u ||
        inputLen > SIZE_MAX / 2u)
        return false;

    output[0] = '\0';

    size_t length       = 0u;
    size_t separatorPos = SIZE_MAX;
    size_t nibbleCount  = inputLen * 2u;
    bool   paddingFound = false;

    for (size_t i = 0u; i < nibbleCount; ++i) {
        uint8_t byte = input[i / 2u];

        uint8_t nibble =
            (i & 1u) ? (uint8_t)(byte & 0x0Fu) : (uint8_t)(byte >> 4u);

        char ch;

        if (nibble <= 9u) {
            ch = (char)('0' + nibble);

        } else if (nibble == 0x0Du) {

            if (separatorPos != SIZE_MAX)
                return false;

            separatorPos = length;
            ch           = '=';

        } else if (nibble == 0x0Fu) {

            /* Padding is valid only in the final low nibble. */
            if (i != nibbleCount - 1u)
                return false;

            ch           = 'F';
            paddingFound = true;

        } else {
            return false;
        }

        /* LEN_MAX_TRACK2 is the maximum logical Track 2 length.
         * Allow one extra character for the padding nibble.
         */
        if (length >= LEN_MAX_TRACK2 + (paddingFound ? 1u : 0u) ||
            length + 1u >= outputSize)
            return false;

        output[length++] = ch;
    }

    /* Validate Track 2 structure, excluding optional F padding. */
    size_t logicalLength = length - (paddingFound ? 1u : 0u);

    if (separatorPos == SIZE_MAX || separatorPos == 0u || separatorPos > 19u ||
        logicalLength < separatorPos + 1u + 4u + 3u)
        return false;

    output[length] = '\0';

    return true;
}

static int parse2Digits(const unsigned char* p) {
    char buf[3] = {(char)p[0], (char)p[1], '\0'};
    int  dig;
    STRING_TO_U8(buf, (uint8_t*)&dig);
    return dig;
}

static int8_t pedDecrypt(void* buffer, int bufLen, void* decryptedData) {
    RETURN_VALUE_IF_NULL(buffer, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(decryptedData, ;, ERR_NULL_PARAMETER);
    PedErr_t result =
        OOP_CALL(ped(), encryptAccountData, buffer, bufLen, decryptedData);
    return result == PED_ERR_OK ? ERR_OK : ERR_NOK;
}

static int dtSetSystemDateTime(const int input[6]) {
    uint8_t dt[7] = {0};

    for (int i = 0; i < 6; i++) {
        dt[i] = toBcd(input[i]);
    }

    return OOP_CALL(sys(), setDateTimeBcd, dt);
}

static void handleMerchantName(LtvStructInfo* tag) {
    DEFINE_BYTE_ARRAY(tmpBuffer, 512);

    memset(settings()->terminal.merchantName, 0,
           sizeof(settings()->terminal.merchantName));

    hexStringToBytes(tag->data, tag->len - 1, tmpBuffer);

    char*  end = strchr((char*)tmpBuffer, '\\');
    size_t len =
        end ? (size_t)(end - (char*)tmpBuffer) : strlen((char*)tmpBuffer);

    LOG_TRACE("merchantFa (%zu): %.*s", len, (int)len, tmpBuffer);

    convertStrNoNumber((char*)tmpBuffer, (int)len, 0, 4, 5,
                       settings()->terminal.merchantName);

    LOG_TRACE("merchantName (%zu): %s",
              strlen(settings()->terminal.merchantName),
              settings()->terminal.merchantName);
}

static void handleMerchantPhone(LtvStructInfo* tag) {
    memset(settings()->terminal.merchantPhone, 0x00,
           sizeof(settings()->terminal.merchantPhone));
    hexStringToBytes(tag->data, tag->len - 1,
                     (unsigned char*)settings()->terminal.merchantPhone);
    LOG_TRACE("MerchantPhone [%s]", settings()->terminal.merchantPhone);
}

static void handleSwitchDateTime(LtvStructInfo* tag) {
    int           dateTime[6] = {0};
    unsigned char tempBuf[64] = {0};

    hexStringToBytes(tag->data, tag->len - 1, tempBuf);

    for (int i = 0; i < 6; i++) {
        dateTime[i] = parse2Digits(tempBuf + 2 + (i * 2));
    }

    dtSetSystemDateTime(dateTime);
}

static void handleVoucherSerial(LtvStructInfo* tag, TxnData* txn) {
    hexStringToBytes(tag->data, tag->len - 1,
                     txn->extention.charge.chargeSerial);
    LOG_TRACE("charge serial = %s", txn->extention.charge.chargeSerial);
}

static void handleVoucherPin(LtvStructInfo* tag, TxnData* txn) {
    if (!tag || !txn) {
        LOG_TRACE("handleVoucherPin: rejected: tag or txn is NULL");
        return;
    }
    TRACE_POINT;
    char          temp[128] = {0};
    int           pinLen    = 0;
    unsigned char pinTemp[64];
    unsigned char pinBytes[32];
    unsigned char pinData[32];
    unsigned int  outLen = 0;
    TRACE_POINT;
    strcpy(temp, tag->data);
    TRACE_POINT;
    pinLen = strlen(temp) / 2;
    memset(pinTemp, 0x00, sizeof(pinTemp));
    memset(pinBytes, 0x00, sizeof(pinBytes));
    memset(pinData, 0x00, sizeof(pinData));
    TRACE_POINT;
    hexStringToBytes(temp, pinLen, pinTemp); // Pin
    TRACE_POINT;
    pinLen /= 2;
    hex2data(pinBytes, pinTemp, pinLen);
    TRACE_POINT;
    pedDecrypt(pinBytes, 16, pinData);
    TRACE_POINT;
    strcpy(txn->extention.charge.chargePin, (const char*)pinData);
    TRACE_POINT;
    LOG_TRACE("charge pin = %s", pinData);
}

static void handleMerchantUniqueId(LtvStructInfo* tag) {
    memset(settings()->terminal.merchantUniqueId, 0x00,
           sizeof(settings()->terminal.merchantUniqueId));
    hexStringToBytes(tag->data, tag->len - 1,
                     (unsigned char*)settings()->terminal.merchantUniqueId);
    LOG_TRACE("settings()->terminal.merchantUniqueId = %s",
              settings()->terminal.merchantUniqueId);
}

void decodeMerchantDesc(char* buffer, TxnData* txn) {
    LtvStructInfo tags[15] = {0};

    int count = unpackLtv(buffer, tags);

    for (int i = 0; i < count; i++) {
        uint16_t tag;
        STRING_TO_U16(tags[i].tag, (uint16_t*)&tag);
        LOG_TRACE("decodeMerchantDesc: i = %d, tag = %s , itag = %d", i,
                  tags[i].tag, tag);
        switch (tag) {
        case TAG_MERCHANT_NAME:
            handleMerchantName(&tags[i]);
            break;

        case TAG_MERCHANT_PHONE:
            handleMerchantPhone(&tags[i]);
            break;

        case TAG_SWITCH_DATETIME:
            handleSwitchDateTime(&tags[i]);
            break;
        case TAG_VOUCHER_SERIAL:
            handleVoucherSerial(&tags[i], txn);
            break;
        case TAG_VOUCHER_PIN:
            handleVoucherPin(&tags[i], txn);
            break;

        case TAG_FORCE_TMS:
            settings()->server.forceTMS = 1;
            break;

        case TAG_MERCHANT_UNIQUE_ID:
            handleMerchantUniqueId(&tags[i]);
            break;

        default:
            LOG_TRACE("Unknown tag %d", tag);
            break;
        }
    }
}

/*********************************************************************************************
 *                                                                                           *
 *                                ISO Setter Helpers
 *                                                                                           *
 ********************************************************************************************/

Error_t setMti(uint16_t mti) {
    DEFINE_STRING(mtistr, (LEN_MAX_MTI + 1));
    prependZerosInt(mti, LEN_MAX_MTI, mtistr, sizeof(mtistr));
    mtistr[LEN_MAX_MTI] = 0;
    iso8583()->setMTI((const DL_UINT8*)mtistr);
    return ERR_OK;
}

static int8_t setBit2(TxnData* data, void* userData) {
    (void)data;
    LOG_TRACE("| Bit 2 - (Pan) | -> %s", data->core.pan);
    iso8583()->setStr(ELEMENT_PAN, data->core.pan);
    return ERR_OK;
}

static int8_t setBit3(TxnData* data, void* userData) {
    uint32_t code = data->core.processCode;
    DEFINE_STRING(prcode, (LEN_MAX_PRCODE + 1));
    prependZerosInt(code, LEN_MAX_PRCODE, prcode, sizeof(prcode));
    prcode[LEN_MAX_STAN] = 0;
    LOG_TRACE("| Bit 3 - (Process Code) | -> %s", prcode);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8*)prcode);
    return ERR_OK;
}

static int8_t setBit4(TxnData* data, void* userData) {
    DEFINE_STRING(amount, (LEN_MAX_AMOUNT + 1));
    prependZerosInt(data->core.amount, LEN_MAX_AMOUNT, amount, sizeof(amount));
    LOG_TRACE("| Bit 4 - (Txn Amount) | -> %s", amount);
    iso8583()->setStr(ELEMENT_AMOUNT_TRANSACTION, (const DL_UINT8*)amount);
    return ERR_OK;
}

static int8_t setBit11(TxnData* data, void* userData) {
    (void)data;
    uint16_t istan = data->core.stan;
    DEFINE_STRING(stan, (LEN_MAX_STAN + 1));
    prependZerosInt(istan, LEN_MAX_STAN, stan, sizeof(stan));
    stan[LEN_MAX_STAN] = 0;
    LOG_TRACE("| Bit 11 - (Stan) | -> %s", stan);
    iso8583()->setStr(ELEMENT_STAN, (const DL_UINT8*)stan);
    return ERR_OK;
}
static int8_t setBit12(TxnData* data, void* userData) {
    (void)data;
    DEFINE_STRING(timeStr, 6 + 1);
    if (data->dateTime == 0) {
        LOG_TRACE(" Setting Bit 13: txn data has no date time information. "
                  "setting current time.");
        DateTime* dt = OOP_CALL(sys(), getDateTime);
        memcpy(timeStr, dt->time, 6);
    } else {
        LOG_TRACE(" Setting Bit 13: txn data has date time information.");
        uint64_t dt = data->dateTime;
        uint32_t itime, idate;
        unpackDateTime(&dt, &idate, &itime);
        timeToRawStr(itime, timeStr, sizeof(timeStr));
    }
    iso8583()->setStr(ELEMENT_TIME_LOCAL_TRANSACTION, (const DL_UINT8*)timeStr);
    LOG_TRACE("| Bit 12 - (Time) | -> %s", timeStr);
    return ERR_OK;
}

static int8_t setBit13(TxnData* data, void* userData) {
    (void)data;
    DEFINE_STRING(dateStr, 4 + 1);
    if (data->dateTime == 0) {
        LOG_TRACE(" Setting Bit 13: txn data has no date time information. "
                  "setting current date.");
        DateTime* dt = OOP_CALL(sys(), getDateTime);
        memcpy(dateStr, dt->date + 2, 4);
    } else {
        LOG_TRACE(" Setting Bit 13: txn data has date time information.");
        DEFINE_STRING(dateTmp, 12);
        uint64_t dt = data->dateTime;
        uint32_t itime, idate;
        unpackDateTime(&dt, &idate, &itime);
        dateToRawStr(idate, dateTmp, sizeof(dateTmp));
        memcpy(dateStr, dateTmp + 4, 4);
    }
    iso8583()->setStr(ELEMENT_DATE_LOCAL_TRANSACTION, (const DL_UINT8*)dateStr);
    LOG_TRACE("| Bit 13 - (Date) | -> %s", dateStr);
    return ERR_OK;
}

static int8_t setBit22(TxnData* data, void* userData) {
    (void)data;
    char* pem;
    if (data->core.txnType == TXN_KAHROBA_PURCHASE) {
        pem = "070";
    } else {
        pem = "021";
    }
    iso8583()->setStr(ELEMENT_POS_ENTRY_MODE, pem);
    LOG_TRACE("| Bit 22 - (Pos Entry Mode) | -> %s", pem);
    return ERR_OK;
}

static int8_t setBit23(TxnData* data, void* userData) {
    (void)data;
    iso8583()->setStr(ELEMENT_CARD_SEQUENCE_NUMBER, "000");
    LOG_TRACE("| Bit 23 - Card Sequence NUmber) | -> %s", "000");
    return ERR_OK;
}

static int8_t setBit24(TxnData* data, void* userData) {
    (void)data;
    DEFINE_STRING(nii, 8);
    prependZerosInt(settings()->server.mainServerNii, 4, nii, sizeof(nii));
    iso8583()->setStr(ELEMENT_NETWORK_INTL_ID, (const DL_UINT8*)nii);
    LOG_TRACE("| Bit 24 - (NII) | -> %s", nii);
    return ERR_OK;
}

static int8_t setBit25(TxnData* data, void* userData) {
    (void)data;
    char* pcc;
    if (data->core.txnType == TXN_KAHROBA_PURCHASE) {
        pcc = "00";
    } else {
        pcc = "14";
    }
    iso8583()->setStr(ELEMENT_POS_CONDITION_CODE, pcc);
    LOG_TRACE("| Bit 25 - (PCC) | -> %s", pcc);
    return ERR_OK;
}

static int8_t setBit32(TxnData* data, void* userData) {
    (void)data;
    char* IIN = strlen(settings()->terminal.acquirerIIN) != 0
                    ? settings()->terminal.acquirerIIN
                    : "000000000";
    iso8583()->setStr(ELEMENT_ACQUIRING_INSTITUTION_ID, (const DL_UINT8*)IIN);
    LOG_TRACE("| Bit 32 - (IIN) | -> %s", IIN);
    return ERR_OK;
}

static int8_t setBit35(TxnData* data, void* userData) {
    if (!data || !userData) {
        LOG_TRACE("ISO Bit35: missing transaction/card context");
        return ERR_NOK;
    }

    /* NFC / Kahroba */
    if (data->core.txnType == TXN_KAHROBA_PURCHASE) {
        const KahrobaTxnSession* session = (const KahrobaTxnSession*)userData;

        if (!session || !session->emvReady) {
            LOG_TRACE("ISO Bit35: EMV context unavailable");
            return ERR_NOK;
        }

        const KahrobaCardData* card = &session->emv.card;

        if (card->track2Len == 0u || card->track2Len > sizeof(card->track2)) {
            LOG_TRACE("ISO Bit35: invalid EMV Track2 length");
            return ERR_NOK;
        }

        char track2[LEN_MAX_TRACK2 + 2u] = {0};

        if (!emvTrack2ToString(session->emv.card.track2,
                               session->emv.card.track2Len, track2,
                               sizeof(track2))) {

            LOG_TRACE("ISO Bit35: EMV Track2 conversion failed");
            return ERR_NOK;
        }

        iso8583()->setStr(ELEMENT_TRACK2, (const DL_UINT8*)track2);

        LOG_TRACE("| Bit 35 - (Track2:NFC) | -> %s", track2);

        /* Clear the temporary sensitive string. */
        volatile char* p = (volatile char*)track2;

        for (size_t i = 0u; i < sizeof(track2); ++i)
            p[i] = 0;

        return ERR_OK;
    }

    /* Magnetic card: use data captured during card input. */
    const MagCardTxnContext* mag = (const MagCardTxnContext*)userData;

    if (!mag->ready || mag->track2Len == 0u ||
        mag->track2Len > LEN_MAX_TRACK2) {

        LOG_TRACE("ISO Bit35: invalid magnetic context");
        return ERR_NOK;
    }

    /*
     * Preserve the current legacy magnetic encoding:
     * pad to LEN_MAX_TRACK2 characters, then append 'F'.
     *
     * Confirm this format against your host specification
     * before changing it.
     */
    char track2[LEN_MAX_TRACK2 + 2u] = {0};

    memset(track2, '0', LEN_MAX_TRACK2);
    memcpy(track2, mag->track2, mag->track2Len);

    track2[LEN_MAX_TRACK2]      = 'F';
    track2[LEN_MAX_TRACK2 + 1u] = '\0';

    iso8583()->setStr(ELEMENT_TRACK2, track2);

    LOG_TRACE("ISO Bit35: magnetic Track2 set, source length=%lu",
              (unsigned long)mag->track2Len);

    volatile char* p = (volatile char*)track2;
    for (size_t i = 0u; i < sizeof(track2); ++i)
        p[i] = 0;

    return ERR_OK;
}

static int8_t setBit37(TxnData* data, void* userData) {
    (void)data;
    DEFINE_STRING(rrn, LEN_MAX_UINT_64);
    LOG_DEBUG("rrn before converting  = %llu", data->core.rrn);
    U64_TO_STRING(&data->core.rrn, rrn);
    iso8583()->setStr(ELEMENT_RETRIEVAL_REFERENCE_NUMBER, rrn);
    LOG_TRACE("| Bit 37 - (RRN) | -> %s", rrn);
    return ERR_OK;
}

static int8_t setBit41(TxnData* data, void* userData) {
    (void)data;
    iso8583()->setStr(ELEMENT_TERMINAL_ID, settings()->terminal.terminalId);
    LOG_TRACE("| Bit 41 - (Terminal ID) | -> %s",
              settings()->terminal.terminalId);
    return ERR_OK;
}
static int8_t setBit42(TxnData* data, void* userData) {
    (void)data;
    iso8583()->setStr(ELEMENT_CARD_ACCEPTOR_ID,
                      settings()->terminal.merchantId);
    LOG_TRACE("| Bit 42 - (Merchant ID) | -> %s",
              settings()->terminal.merchantId);
    return ERR_OK;
}

static int8_t buildChargeF48(TxnData* data, const char* sn, char* privateData) {
    DEFINE_STRING(code, 32);
    DEFINE_STRING(opCode, 4);
    uint16_t op = data->extention.charge.op;
    U16_TO_STRING((uint16_t*)&op, opCode);
    if (data->core.processCode == PRC_VOUCHER) {
        uint64_t amount = data->core.amount;
        int      count  = 0;
        while (amount > 0) {
            if ((amount % 10) == 0) {
                count += 1;
                amount = amount / 10;
            } else {
                break;
            }
        }
        sprintf(code, "%s%d%ld", opCode, count, amount);
        setVoucherLtv(sn, PNA_APP_VERSION, 0 /*language*/, code, privateData);
    } else if (data->core.processCode == PRC_TOPUP) {
        sprintf(code, "%s0", opCode);
        LOG_TRACE("Phone number = %s", data->extention.charge.phoneNumber);
        setTopupLtv(sn, PNA_APP_VERSION, 0 /*language*/, code,
                    data->extention.charge.phoneNumber, privateData);
    }
}

static int8_t buildBillF48(TxnData* data, const char* sn, char* privateData) {
    setBillLtv(sn, PNA_APP_VERSION, 0 /*language*/, data->extention.bill.billId,
               data->extention.bill.paymentId, privateData);
}

static int8_t setBit48(TxnData* data, void* userData) {
    (void)data;
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    DEFINE_STRING(privateData, 128);
    if (data->core.processCode == PRC_BILL_PAYMENT) {
        buildBillF48(data, sn, privateData);
    } else if (data->core.processCode == PRC_VOUCHER ||
               data->core.processCode == PRC_TOPUP) {
        buildChargeF48(data, sn, privateData);
    } else {
        setCommonLtv(sn, PNA_APP_VERSION, 0 /*language*/, privateData);
    }
    iso8583()->setStr(ELEMENT_ADDITIONAL_DATA_PRIVATE,
                      (const DL_UINT8*)privateData);
    LOG_TRACE("Bit 48 | (Private Data): %s", privateData);
    return ERR_OK;
}

static int8_t setBit49(TxnData* data, void* userData) {
    (void)data;
    char* currency = "364";
    iso8583()->setStr(ELEMENT_CURRENCY_CODE_TRANSACTION,
                      (const DL_UINT8*)currency);
    LOG_TRACE("Bit 49 | (Currency Code): %s", currency);
    return ERR_OK;
}

static int8_t setBit52(TxnData* data, void* userData) {
    (void)data;
    /* NFC / Kahroba */
    if (data->core.txnType == TXN_KAHROBA_PURCHASE) {
        const KahrobaTxnSession* session = (const KahrobaTxnSession*)userData;

        if (!session || !session->emvReady) {
            LOG_TRACE("ISO Bit52: EMV context unavailable");
            return ERR_NOK;
        }

        if (!*session->hostPinRequired) {
            return ERR_OK;
        }
    }

    DEFINE_STRING(pinblock, 24);
    LOG_TRACE("ISO Bit52: get pinblock for pan = %s", data->core.pan);
    PedErr_t pederr = OOP_CALL(ped(), getPinBlock, data->core.pan, pinblock,
                               sizeof(pinblock));
    RETURN_VALUE_IF_NOT(pederr, PED_ERR_OK, ;, ERR_NOK);
    iso8583()->setBin(ELEMENT_PIN_DATA, pinblock, LEN_MAX_PIN_BLOCK);
    DEFINE_STRING(pbh, 24);
    bytesToHex(pinblock, 8, pbh, sizeof(pbh));
    LOG_TRACE("Bit 52 | (Pin Block): %s", pbh);
    return ERR_OK;
}

static int8_t setBit53(TxnData* data, void* userData) {
    (void)data;
    char* sci;
    if (data->core.txnType == TXN_KAHROBA_PURCHASE) {
        sci = "0800030202031500";
    } else {
        sci = SecRelControlInfo;
    }
    iso8583()->setStr(ELEMENT_SECURITY_CONTROL_INFO, (const DL_UINT8*)sci);
    LOG_TRACE("Bit 53 | (Security Control Info): %s", sci);
    return ERR_OK;
}

/**
 * @brief Set ISO8583 DE55 - Integrated Circuit Card (ICC) Data.
 *
 * For Kahroba transactions, userData points to the active
 * KahrobaEmvContext containing the binary EMV TLV payload.
 *
 * The field must be passed as binary data with an explicit length.
 * Never log its contents.
 *
 * @return ERR_OK on success, ERR_NOK on failure.
 */
static int8_t setBit55(TxnData* data, void* userData) {
    if (!data || !userData) {
        LOG_TRACE("ISO Bit55: invalid arguments");
        return ERR_NOK;
    }

    if (data->core.txnType != TXN_KAHROBA_PURCHASE) {
        LOG_TRACE("ISO Bit55: unsupported transaction type=%d",
                  (int)data->core.txnType);
        return ERR_NOK;
    }

    const KahrobaTxnSession* session = (const KahrobaTxnSession*)userData;

    if (!session->emvReady || session->field55Len == 0u ||
        session->field55Len > sizeof(session->field55) ||
        session->field55Len > UINT16_MAX) {

        LOG_TRACE("ISO Bit55: invalid EMV data");
        return ERR_NOK;
    }

    /* Amount must match the amount used to generate ARQC. */
    if (data->core.amount != session->emv.authorizedAmount) {
        LOG_TRACE("ISO Bit55: transaction amount mismatch");
        return ERR_NOK;
    }

    /* An online authorization request requires ARQC. */
    if ((session->emv.acResult.cid & 0xC0u) != 0x80u) {
        LOG_TRACE("ISO Bit55: expected ARQC, CID=%02X",
                  (unsigned)session->emv.acResult.cid);
        return ERR_NOK;
    }

    /*
     * Pass the original binary TLV payload to DE55.
     *
     * iso8583()->setBin() must accept the complete binary
     * payload, with no ASCII-hex conversion.
     */
    iso8583()->setBin(ELEMENT_ICC_DATA, (const DL_UINT8*)session->field55,
                      (uint16_t)session->field55Len);

    LOG_TRACE("ISO Bit55: set successfully, length=%lu",
              (unsigned long)session->field55Len);

    return ERR_OK;
}

static int8_t getBit48(TxnData* data, void* userData) {
    DEFINE_STRING(feild, 1028);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild, data);
    return ERR_OK;
}

static int8_t setMac(TxnData* data, uint8_t feild) {
    (void)data;
    DEFINE_BYTE_ARRAY(mac, LEN_MAX_ISO_MAC + 1);
    DEFINE_BYTE_ARRAY(macHex, LEN_MAX_ISO_MAC + 1);
    iso8583()->setBin(feild, (const DL_UINT8*)mac, LEN_MAX_ISO_MAC);
    DEFINE_BYTE_ARRAY(tmpBuf, ISO_MAX_BUFFER);
    size_t packedLen;
    RETURN_VALUE_IF_NOT(iso8583()->pack(tmpBuf, &packedLen), ISO_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT((packedLen < LEN_MAX_ISO_MAC), false, ;, ERR_NOK);
    LOG_TRACE("ISO Build mac: packed len = %u", packedLen);
    PedErr_t pedErr =
        ped()->getMac(16, tmpBuf, packedLen - LEN_MAX_ISO_MAC, mac);
    RETURN_VALUE_IF_NOT(pedErr, PED_ERR_OK, ;, ERR_NOK);
    bytesToHex(mac, 4, macHex, sizeof(macHex));
    LOG_TRACE("ISO Build mac: mac hex = %s", macHex);
    iso8583()->setBin(feild, (const DL_UINT8*)macHex, LEN_MAX_ISO_MAC);
    return ERR_OK;
}

static int8_t setBit64(TxnData* data, void* userData) {
    return setMac(data, ELEMENT_MAC);
}

// static int8_t setBit90(TxnData* data) {
//     DEFINE_STRING(mtistr, (LEN_MAX_MTI + 1));
//     prependZerosInt(data->core.mti, LEN_MAX_MTI, mtistr, sizeof(mtistr));
//     mtistr[LEN_MAX_MTI] = 0;

//     DEFINE_STRING(stan, (LEN_MAX_STAN + 1));
//     uint16_t istan = data->core.stan;
//     prependZerosInt(istan, LEN_MAX_STAN, stan, sizeof(stan));
//     stan[LEN_MAX_STAN] = 0;

//     uint32_t date, time;
//     uint64_t dateTime = data->dateTime;
//     DEFINE_STRING(dtStr, 24);
//     unpackDateTime(&dateTime, &date, &time);
//     dateTimeToRawStr(date, time, dtStr, sizeof(dtStr));

//     DEFINE_STRING(paddedIin, 12);
//     if (strlen(settings()->terminal.acquirerIIN) > 0) {
//         snprintf(paddedIin, sizeof(paddedIin), "%011s",
//                  settings()->terminal.acquirerIIN);
//     } else {
//         snprintf(paddedIin, sizeof(paddedIin), "%s", "00000000000");
//     }

//     DEFINE_STRING(forwardingId, 12);
//     snprintf(forwardingId, sizeof(forwardingId), "%s", "00000");
//     memcpy(forwardingId, data->core.pan, 6);

//     DEFINE_STRING(feild, 128);
//     int idx = 0;
//     memcpy(feild, mtistr, 4);
//     idx += 4;
//     memcpy(feild + idx, stan, LEN_MAX_STAN);
//     idx += LEN_MAX_STAN;
//     memcpy(feild + idx, dtStr + 4, 10);
//     idx += 10;
//     memcpy(feild + idx, paddedIin, 11);
//     idx += 11;
//     memcpy(feild + idx, forwardingId, 11);

//     iso8583()->setStr(ELEMENT_ORIGINAL_DATA_ELEMENTS, (const
//     DL_UINT8*)feild); LOG_TRACE("Bit 90 | (Original Data Elements): %s",
//     feild); return ERR_OK;
// }

/*
 * Format an institution ID as eleven numeric characters.
 * Empty IDs are rejected; supply "00000000000" explicitly if the
 * protocol permits an absent institution ID to be represented by zeros.
 */
static bool formatBit90Id(const char* source, char out[12]) {
    if (!source) {
        return false;
    }

    size_t length = 0;

    while (source[length] != '\0') {
        if (length >= 11 || source[length] < '0' || source[length] > '9') {
            return false;
        }

        ++length;
    }

    if (length == 0) {
        return false;
    }

    memset(out, '0', 11);
    memcpy(out + 11 - length, source, length);
    out[11] = '\0';

    return true;
}

/*
 * Set field 90 from the ORIGINAL transaction.
 *
 * data->core.mti must still contain the original MTI.
 * data->core.stan must still contain the original STAN.
 *
 * Assumes dateTimeToRawStr() produces YYYYMMDDhhmmss.
 * The resulting string is checked before extracting MMDDhhmmss.
 *
 * acquirerIin:  original field 32.
 * forwardingId: original field 33.
 */
static int8_t setBit90(TxnData* data, void* userData) {
    if (!data) {
        LOG_TRACE("Bit90: original transaction is NULL");
        return ERR_NOK;
    }

    /* Validate before formatting; never truncate original identifiers. */
    if ((uint64_t)data->core.stan > 999999ULL) {
        LOG_TRACE("Bit90: original MTI or STAN exceeds field width");
        return ERR_NOK;
    }

    char mti[5];
    char stan[7];

    if (snprintf(mti, sizeof(mti), "%04lu", (unsigned long)200) != 4 ||
        snprintf(stan, sizeof(stan), "%06lu", (unsigned long)data->core.stan) !=
            6) {
        LOG_TRACE("Bit90: MTI or STAN formatting failed");
        return ERR_NOK;
    }

    uint32_t date           = 0;
    uint32_t time           = 0;
    uint64_t packedDateTime = data->dateTime;

    char dateTimeText[24] = {0};

    unpackDateTime(&packedDateTime, &date, &time);
    dateTimeToRawStr(date, time, dateTimeText, sizeof(dateTimeText));

    /* Require exactly YYYYMMDDhhmmss before removing the year. */
    for (size_t i = 0; i < 14; ++i) {
        if (dateTimeText[i] < '0' || dateTimeText[i] > '9') {
            LOG_TRACE("Bit90: invalid original date/time format");
            return ERR_NOK;
        }
    }

    if (dateTimeText[14] != '\0') {
        LOG_TRACE("Bit90: original date/time must contain fourteen digits");
        return ERR_NOK;
    }

    char acquiring[12];
    char forwarding[12];

    char* acquirerIin = strlen(settings()->terminal.acquirerIIN) != 0
                            ? settings()->terminal.acquirerIIN
                            : "000000000";

    DEFINE_STRING(forwardingId, 6 + 1);
    memcpy(forwardingId, data->core.pan, 6);

    if (!formatBit90Id(acquirerIin, acquiring)) {
        LOG_TRACE("Bit90: invalid original field 32");
        return ERR_NOK;
    }

    if (!formatBit90Id(forwardingId, forwarding)) {
        LOG_TRACE("Bit90: invalid original field 33");
        return ERR_NOK;
    }

    char field[43];

    memcpy(field, mti, 4);
    memcpy(field + 4, stan, 6);
    memcpy(field + 10, dateTimeText + 4, 10);
    memcpy(field + 20, acquiring, 11);
    memcpy(field + 31, forwarding, 11);

    field[42] = '\0';

    /*
     * Uses your existing setter API.
     * If setStr() returns a status, check and propagate its failure.
     */
    iso8583()->setStr(ELEMENT_ORIGINAL_DATA_ELEMENTS, (const DL_UINT8*)field);

    LOG_TRACE("Bit 90 | (Original Data Elements): %s", field);

    return ERR_OK;
}

static int8_t setBit116(TxnData* data, void* userData) {
    (void)data;
    char* rn = "73000d02116";
    iso8583()->setStr(ELEMENT_RESERVED_NATIONAL_116, (const DL_UINT8*)rn);
    LOG_TRACE("Bit 116 | (Reserved National): %s", rn);
    return ERR_OK;
}

static int8_t setBit128(TxnData* data, void* userData) {
    return setMac(data, ELEMENT_MAC_2);
}

const IsoFeildsFunc isoHelper[ISO_MAX_FIELDS] = {
    /******************************************************************/
    /*BIT 2: ELEMENT_PAN */
    /******************************************************************/
    {.feild = 2, .set = setBit2, .get = NULL},
    /******************************************************************/
    /*BIT 3: ELEMENT_PROCESSING_CODE */
    /******************************************************************/
    {.feild = 3, .set = setBit3, .get = NULL},
    /******************************************************************/
    /*BIT 4: ELEMENT_AMOUNT_TRANSACTION */
    /******************************************************************/
    {.feild = 4, .set = setBit4, .get = NULL},
    /******************************************************************/
    /*BIT 11: ELEMENT_STAN */
    /******************************************************************/
    {.feild = 11, .set = setBit11, .get = NULL},
    /******************************************************************/
    /*BIT 12: ELEMENT_TIME_LOCAL_TRANSACTION */
    /******************************************************************/
    {.feild = 12, .set = setBit12, .get = NULL},
    /******************************************************************/
    /*BIT 13: ELEMENT_DATE_LOCAL_TRANSACTION */
    /******************************************************************/
    {.feild = 13, .set = setBit13, .get = NULL},
    /******************************************************************/
    /*BIT 22: ELEMENT_POS_ENTRY_MODE */
    /******************************************************************/
    {.feild = 22, .set = setBit22, .get = NULL},
    /******************************************************************/
    /*BIT 23: ELEMENT_CARD_SEQUENCE_NUMBER */
    /******************************************************************/
    {.feild = 23, .set = setBit23, .get = NULL},
    /******************************************************************/
    /*BIT 24: ELEMENT_NETWORK_INTL_ID */
    /******************************************************************/
    {.feild = 24, .set = setBit24, .get = NULL},
    /******************************************************************/
    /*BIT 25: ELEMENT_POS_CONDITION_CODE */
    /******************************************************************/
    {.feild = 25, .set = setBit25, .get = NULL},
    /******************************************************************/
    /*BIT 32: ELEMENT_ACQUIRING_INSTITUTION_ID */
    /******************************************************************/
    {.feild = 32, .set = setBit32, .get = NULL},
    /******************************************************************/
    /*BIT 35: ELEMENT_TRACK2 */
    /******************************************************************/
    {.feild = 35, .set = setBit35, .get = NULL},
    /******************************************************************/
    /*BIT 37: ELEMENT_RETRIEVAL_REFERENCE_NUMBER */
    /******************************************************************/
    {.feild = 37, .set = setBit37, .get = NULL},
    /******************************************************************/
    /*BIT 41: ELEMENT_TERMINAL_ID */
    /******************************************************************/
    {.feild = 41, .set = setBit41, .get = NULL},
    /******************************************************************/
    /*BIT 42: ELEMENT_CARD_ACCEPTOR_ID */
    /******************************************************************/
    {.feild = 42, .set = setBit42, .get = NULL},
    /******************************************************************/
    /*BIT 48: ELEMENT_ADDITIONAL_DATA_PRIVATE */
    /******************************************************************/
    {.feild = 48, .set = setBit48, .get = NULL},
    /******************************************************************/
    /*BIT 49: ELEMENT_CURRENCY_CODE_TRANSACTION */
    /******************************************************************/
    {.feild = 49, .set = setBit49, .get = NULL},
    /******************************************************************/
    /*BIT 52: ELEMENT_PIN_DATA */
    /******************************************************************/
    {.feild = 52, .set = setBit52, .get = NULL},
    /******************************************************************/
    /*BIT 53: ELEMENT_SECURITY_CONTROL_INFO */
    /******************************************************************/
    {.feild = 53, .set = setBit53, .get = NULL},
    /******************************************************************/
    /* BIT 55: ELEMENT_ICC_DATA */
    /******************************************************************/
    {.feild = ELEMENT_ICC_DATA, .set = setBit55, .get = NULL},
    /******************************************************************/
    /*BIT 64: ELEMENT_MAC */
    /******************************************************************/
    {.feild = 64, .set = setBit64, .get = NULL},
    /******************************************************************/
    /*BIT 90: ELEMENT_ORIGINAL_DATA_ELEMENTS */
    /******************************************************************/
    {.feild = 90, .set = setBit90, .get = NULL},
    /******************************************************************/
    /*BIT 116: ELEMENT_RESERVED_NATIONAL_116 */
    /******************************************************************/
    {.feild = 116, .set = setBit116, .get = NULL},
    /******************************************************************/
    /*BIT 64: ELEMENT_MAC_2 */
    /******************************************************************/
    {.feild = 128, .set = setBit128, .get = NULL},
};
