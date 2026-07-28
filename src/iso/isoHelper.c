#include "isoHelper.h"
#include "sys/sys.h"
#include "settings/settings.h"
#include "ped/ped.h"
#include "utility/ltv.h"
#include "utility/utility.h"
#include "utility/arith.h"
#include "magReader/magReader.h"

enum {
    TAG_MERCHANT_NAME      = 31,
    TAG_MERCHANT_PHONE     = 34,
    TAG_VOUCHER_PIN        = 41,
    TAG_SWITCH_DATETIME    = 50,
    TAG_FORCE_TMS          = 94,
    TAG_MERCHANT_UNIQUE_ID = 98,
};

static int parse2Digits(const unsigned char* p) {
    char buf[3] = {(char)p[0], (char)p[1], '\0'};

    return libAtoi(buf);
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

    LOG_DEBUG("merchantFa (%zu): %.*s", len, (int)len, tmpBuffer);

    convertStrNoNumber((char*)tmpBuffer, (int)len, 0, 4, 5,
                       settings()->terminal.merchantName);

    LOG_DEBUG("merchantName (%zu): %s",
              strlen(settings()->terminal.merchantName),
              settings()->terminal.merchantName);
}

static void handleMerchantPhone(LtvStructInfo* tag) {
    memset(settings()->terminal.merchantPhone, 0x00,
           sizeof(settings()->terminal.merchantPhone));
    hexStringToBytes(tag->data, tag->len - 1,
                     (unsigned char*)settings()->terminal.merchantPhone);
    LOG_DEBUG(" config.MerchantPhone [%s]", settings()->terminal.merchantPhone);
}

static void handleSwitchDateTime(LtvStructInfo* tag) {
    int           dateTime[6] = {0};
    unsigned char tempBuf[64] = {0};

    LOG_DEBUG("dateTime(%d) [%s]", tag->len, tag->data);

    hexStringToBytes(tag->data, tag->len - 1, tempBuf);

    for (int i = 0; i < 6; i++) {
        dateTime[i] = parse2Digits(tempBuf + 2 + (i * 2));
    }

    int ret = dtSetSystemDateTime(dateTime);

    LOG_DEBUG("dtSetSystemDateTime(%s) -> %d", tempBuf, ret);
}

static void handleVoucherPin(LtvStructInfo* tag) {
    char          temp[128] = {0};
    int           pinLen    = 0;
    unsigned char pinTemp[64];
    unsigned char pinBytes[32];
    unsigned char pinData[32];
    unsigned int  outLen = 0;
    strcpy(temp, tag->data);
    pinLen = strlen(temp) / 2;
    LOG_DEBUG("parseVoucherTransactionSIPA::temp[%s]pinLen[%d]", temp, pinLen);
    memset(pinTemp, 0x00, sizeof(pinTemp));
    memset(pinBytes, 0x00, sizeof(pinBytes));
    memset(pinData, 0x00, sizeof(pinData));
    hexStringToBytes(temp, pinLen, pinTemp); // Pin
    pinLen /= 2;
    hex2data(pinBytes, pinTemp, pinLen);
    LOG_DEBUG("parseVoucherTransactionSIPA::pinLen[%d]", pinLen);
    pedDecrypt(pinBytes, 16, pinData);
    // TODO
    // strcpy(currentTransaction.sqlTrx.PaymentId, (const char *)pinData);
    // TraceExt(pinTemp, 16, "VoucherPin [%s]",
    // currentTransaction.sqlTrx.PaymentId);
}

static void handleMerchantUniqueId(LtvStructInfo* tag) {
    memset(settings()->terminal.merchantUniqueId, 0x00,
           sizeof(settings()->terminal.merchantUniqueId));
    hexStringToBytes(tag->data, tag->len - 1,
                     (unsigned char*)settings()->terminal.merchantUniqueId);
    LOG_DEBUG("settings()->terminal.merchantUniqueId = %s",
              settings()->terminal.merchantUniqueId);
}

void decodeMerchantDesc(char* buffer) {
    LtvStructInfo tags[15] = {0};

    int count = unpackLtv(buffer, tags);

    for (int i = 0; i < count; i++) {
        int tag = libAtoi(tags[i].tag);
        LOG_DEBUG("decodeMerchantDesc: i = %d, tag = %s , itag = %d", i,
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

        case TAG_VOUCHER_PIN:
            handleVoucherPin(&tags[i]);
            break;

        case TAG_FORCE_TMS:
            settings()->server.forceTMS = 1;
            break;

        case TAG_MERCHANT_UNIQUE_ID:
            handleMerchantUniqueId(&tags[i]);
            break;

        default:
            LOG_DEBUG("Unknown tag %d", tag);
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

static int8_t setBit2(TxnData* data) {
    (void)data;
    LOG_TRACE("| Bit 2 - (Pan) | -> %s", data->core.pan);
    iso8583()->setStr(ELEMENT_PAN, data->core.pan);
    return ERR_OK;
}

static int8_t setBit3(TxnData* data) {
    uint32_t code = data->core.processCode;
    DEFINE_STRING(prcode, (LEN_MAX_PRCODE + 1));
    prependZerosInt(code, LEN_MAX_PRCODE, prcode, sizeof(prcode));
    prcode[LEN_MAX_STAN] = 0;
    LOG_TRACE("| Bit 3 - (Process Code) | -> %s", prcode);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8*)prcode);
    return ERR_OK;
}

static int8_t setBit4(TxnData* data) {
    DEFINE_STRING(amount, (LEN_MAX_AMOUNT + 1));
    prependZerosInt(data->core.amount, LEN_MAX_AMOUNT, amount, sizeof(amount));
    LOG_TRACE("| Bit 4 - (Txn Amount) | -> %s", amount);
    iso8583()->setStr(ELEMENT_AMOUNT_TRANSACTION, (const DL_UINT8*)amount);
    return ERR_OK;
}

static int8_t setBit11(TxnData* data) {
    (void)data;
    uint16_t istan = data->core.stan;
    DEFINE_STRING(stan, (LEN_MAX_STAN + 1));
    prependZerosInt(istan, LEN_MAX_STAN, stan, sizeof(stan));
    stan[LEN_MAX_STAN] = 0;
    LOG_TRACE("| Bit 11 - (Stan) | -> %s", stan);
    iso8583()->setStr(ELEMENT_STAN, (const DL_UINT8*)stan);
    data->dateTime = OOP_CALL(sys(), getPackedDateTime);
    return ERR_OK;
}
static int8_t setBit12(TxnData* data) {
    (void)data;
    DateTime* dt = OOP_CALL(sys(), getDateTime);
    iso8583()->setStr(ELEMENT_TIME_LOCAL_TRANSACTION,
                      (const DL_UINT8*)dt->time);
    LOG_TRACE("| Bit 12 - (Time) | -> %s", dt->time);
    data->dateTime = OOP_CALL(sys(), getPackedDateTime);
    return ERR_OK;
}
static int8_t setBit13(TxnData* data) {
    (void)data;
    DateTime* dt = OOP_CALL(sys(), getDateTime);
    iso8583()->setStr(ELEMENT_DATE_LOCAL_TRANSACTION,
                      (const DL_UINT8*)dt->date + 2);
    LOG_TRACE("| Bit 13 - (Date) | -> %s", dt->date + 2);
    data->dateTime = OOP_CALL(sys(), getPackedDateTime);
    return ERR_OK;
}
static int8_t setBit22(TxnData* data) {
    (void)data;
    iso8583()->setStr(ELEMENT_POS_ENTRY_MODE, "021");
    LOG_TRACE("| Bit 22 - (Pos Entry Mode) | -> %s", "021");
    return ERR_OK;
}
static int8_t setBit24(TxnData* data) {
    (void)data;
    DEFINE_STRING(nii, 8);
    prependZerosInt(settings()->server.mainServerNii, 4, nii, sizeof(nii));
    iso8583()->setStr(ELEMENT_NETWORK_INTL_ID, (const DL_UINT8*)nii);
    LOG_TRACE("| Bit 24 - (NII) | -> %s", nii);
    return ERR_OK;
}
static int8_t setBit25(TxnData* data) {
    (void)data;
    iso8583()->setStr(ELEMENT_POS_CONDITION_CODE, "14");
    LOG_TRACE("| Bit 25 - (PCC) | -> %s", "14");
    return ERR_OK;
}

static int8_t setBit32(TxnData* data) {
    (void)data;
    char* IIN = strlen(settings()->terminal.acquirerIIN) != 0
                    ? settings()->terminal.acquirerIIN
                    : "000000000";
    iso8583()->setStr(ELEMENT_ACQUIRING_INSTITUTION_ID, (const DL_UINT8*)IIN);
    LOG_TRACE("| Bit 32 - (IIN) | -> %s", IIN);
    return ERR_OK;
}

static int8_t setBit35(TxnData* data) {
    (void)data;
    TrackData_t track2 = OOP_CALL(magreader(), getTrack2);
    DEFINE_STRING(t2, (LEN_MAX_TRACK2 + 10));

    LOG_DEBUG("track2 len = %d", track2.len);
    padRight(track2.data, track2.len, LEN_MAX_TRACK2, t2, '0');
    strcat(t2, "F");
    iso8583()->setStr(ELEMENT_TRACK2, t2);
    LOG_TRACE("| Bit 35 - (Track2) | -> %s", t2);
    return ERR_OK;
}

static int8_t setBit37(TxnData* data) {
    (void)data;
    DEFINE_STRING(rrn, LEN_MAX_UINT_64);
    LOG_DEBUG("rrn before converting  = %llu", data->core.rrn);
    U64_TO_STRING(&data->core.rrn, rrn);
    iso8583()->setStr(ELEMENT_RETRIEVAL_REFERENCE_NUMBER, rrn);
    LOG_TRACE("| Bit 37 - (RRN) | -> %s", rrn);
    return ERR_OK;
}

static int8_t setBit41(TxnData* data) {
    (void)data;
    iso8583()->setStr(ELEMENT_TERMINAL_ID, settings()->terminal.terminalId);
    LOG_TRACE("| Bit 41 - (Terminal ID) | -> %s",
              settings()->terminal.terminalId);
    return ERR_OK;
}
static int8_t setBit42(TxnData* data) {
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
    U16_TO_STRING((uint16_t*)&data->extention.charge.op, opCode);
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
        setTopupLtv(sn, PNA_APP_VERSION, 0 /*language*/, code,
                    data->extention.charge.phoneNumber, privateData);
    }
}

static int8_t buildBillF48(TxnData* data, const char* sn, char* privateData) {
    setBillLtv(sn, PNA_APP_VERSION, 0 /*language*/, data->extention.bill.billId,
               data->extention.bill.paymentId, privateData);
}

static int8_t setBit48(TxnData* data) {
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

static int8_t setBit49(TxnData* data) {
    (void)data;
    char* currency = "364";
    iso8583()->setStr(ELEMENT_CURRENCY_CODE_TRANSACTION,
                      (const DL_UINT8*)currency);
    LOG_TRACE("Bit 49 | (Currency Code): %s", currency);
    return ERR_OK;
}

static int8_t setBit52(TxnData* data) {
    (void)data;
    DEFINE_STRING(pan, 24);
    DEFINE_STRING(pinblock, 24);
    magreader()->getPan(pan, sizeof(pan));
    PedErr_t pederr =
        OOP_CALL(ped(), getPinBlock, pan, pinblock, sizeof(pinblock));
    RETURN_VALUE_IF_NOT(pederr, PED_ERR_OK, ;, ERR_NOK);
    iso8583()->setBin(ELEMENT_PIN_DATA, pinblock, LEN_MAX_PIN_BLOCK);
    LOG_TRACE("Bit 52 | (Pin Block): %s", pinblock);
    return ERR_OK;
}
static int8_t setBit53(TxnData* data) {
    (void)data;
    iso8583()->setStr(ELEMENT_SECURITY_CONTROL_INFO,
                      (const DL_UINT8*)SecRelControlInfo);
    LOG_TRACE("Bit 53 | (Security Control Info): %s", SecRelControlInfo);
    return ERR_OK;
}

static int8_t getBit48(TxnData* data) {
    DEFINE_STRING(feild, 1028);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild);
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

static int8_t setBit64(TxnData* data) { return setMac(data, ELEMENT_MAC); }

static int8_t setBit90(TxnData* data) { return setMac(data, ELEMENT_MAC_2); }

static int8_t setBit128(TxnData* data) { return setMac(data, ELEMENT_MAC_2); }

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
    /*BIT 64: ELEMENT_MAC */
    /******************************************************************/
    {.feild = 64, .set = setBit64, .get = NULL},
    /******************************************************************/
    /*BIT 90: ELEMENT_ORIGINAL_DATA_ELEMENTS */
    /******************************************************************/
    {.feild = 90, .set = setBit90, .get = NULL},
    /******************************************************************/
    /*BIT 64: ELEMENT_MAC_2 */
    /******************************************************************/
    {.feild = 128, .set = setBit128, .get = NULL},
};