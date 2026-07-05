#include "isoBuilder.h"
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

/*********************************************************************************************
 *                                                                                           *
 *                                      Helpers              *
 *                                                                                           *
 ********************************************************************************************/

static int parse2Digits(const unsigned char* p) {
    char buf[3] = {(char)p[0], (char)p[1], '\0'};

    return libAtoi(buf);
}

static int8_t compareMac(int keyIndex, char* data) {
    RETURN_VALUE_IF_NULL(data, ;, ERR_NULL_PARAMETER);
    uint16_t packedSize = data[0] * 256 + data[1];
    packedSize          = packedSize - 5; // without header
    DL_UINT8 packBufTemp[2048];
    memset(packBufTemp, 0x00, sizeof(packBufTemp));
    memcpy(packBufTemp, data + 7, packedSize);

    DEFINE_BYTE_ARRAY(mac, 9);
    DEFINE_BYTE_ARRAY(asciiMac, 9);
    size_t length = sizeof(mac);
    if (iso8583()->getBin(ELEMENT_MAC_2, mac, &length) == ISO_OK) {
    } else if (iso8583()->getBin(ELEMENT_MAC, mac, &length) == ISO_OK) {
    } else {
        LOG_DEBUG("mac:::::::::NO MAC !!!!");
        return ERR_NOK;
    }
    LOG_DEBUG("[mac (%d)] [%s]", length, mac);
    // pedGetMacAscii(keyIndex, 16, packBufTemp, packedSize - 8, (char
    // *)asciiMac);
    int8_t macCompare = memcmp(mac, asciiMac, 8);
    if (macCompare != 0) {
        LOG_DEBUG("mac:::::::::check mac failed !!!!");
        return ERR_NOK;
    }
    LOG_DEBUG("mac:::::::::check mac succeed !!!!");
    return ERR_OK;
}

static int dtSetSystemDateTime(const int input[6]) {
    uint8_t dt[7] = {0};

    for (int i = 0; i < 6; i++) {
        dt[i] = toBcd(input[i]);
    }

    return OOP_CALL(sys(), setDateTimeBcd, dt);
}

static int8_t pedDecrypt(void* buffer, int bufLen, void* decryptedData) {
    RETURN_VALUE_IF_NULL(buffer, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(decryptedData, ;, ERR_NULL_PARAMETER);
    PedErr_t result =
        OOP_CALL(ped(), encryptAccountData, buffer, bufLen, decryptedData);
    return result == PED_ERR_OK ? ERR_OK : ERR_NOK;
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

static void decodeMerchantDesc(char* buffer) {
    LtvStructInfo tags[15] = {0};

    int count = unpackLtv(buffer, tags);

    for (int i = 0; i < count; i++) {
        int tag = libAtoi(tags[i].tag);

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

static inline Error_t setDateTime() {
    DateTime* dt = OOP_CALL(sys(), getDateTime);
    iso8583()->setStr(ELEMENT_TIME_LOCAL_TRANSACTION,
                      (const DL_UINT8*)dt->time);
    iso8583()->setStr(ELEMENT_DATE_LOCAL_TRANSACTION,
                      (const DL_UINT8*)dt->date + 2);
    return ERR_OK;
}

static inline Error_t setStan(uint32_t istan) {
    DEFINE_STRING(stan, (STAN_SIZE + 1));
    prependZerosInt(istan, STAN_SIZE, stan, sizeof(stan));
    stan[STAN_SIZE] = 0;
    LOG_DEBUG("stan = %d, stan string = %s", istan, stan);
    iso8583()->setStr(ELEMENT_STAN, (const DL_UINT8*)stan);
    return ERR_OK;
}

static inline Error_t setNii() {
    DEFINE_STRING(nni, 8);
    prependZerosInt(settings()->server.mainServerNii, 4, nni, sizeof(nni));
    iso8583()->setStr(ELEMENT_NETWORK_INTL_ID, (const DL_UINT8*)nni);
    return ERR_OK;
}

static inline Error_t setTerminalNum() {
    iso8583()->setStr(ELEMENT_TERMINAL_ID,
                      (const DL_UINT8*)settings()->terminal.terminalId);
    return ERR_OK;
}

static inline Error_t setSecRelCtrlInfo() {
    iso8583()->setStr(ELEMENT_SECURITY_CONTROL_INFO,
                      (const DL_UINT8*)SecRelControlInfo);
    return ERR_OK;
}

static inline Error_t setPrCode(uint32_t code) {
    DEFINE_STRING(prcode, (PRCODE_SIZE + 1));
    prependZerosInt(code, PRCODE_SIZE, prcode, sizeof(prcode));
    prcode[STAN_SIZE] = 0;
    LOG_DEBUG("stan = %d, stan string = %s", code, prcode);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8*)prcode);
    return ERR_OK;
}

static inline Error_t setMti(uint16_t mti) {
    DEFINE_STRING(mtistr, (MTI_SIZE + 1));
    prependZerosInt(mti, MTI_SIZE, mtistr, sizeof(mtistr));
    mtistr[MTI_SIZE] = 0;
    iso8583()->setMTI((const DL_UINT8*)mtistr);
    return ERR_OK;
}

static Error_t setWorkingKeys() {
    DEFINE_BYTE_ARRAY(keys, 512);
    size_t      size = sizeof(keys);
    IsoStatus_t ret =
        iso8583()->getBin(ELEMENT_RESERVED_PRIVATE_62, keys, &size);
    RETURN_VALUE_IF_NOT(ret, ISO_OK, ;, ERR_NOK);
    LOG_DEBUG("Keys length = %d, keys = %s", size, keys);
    DEFINE_BYTE_ARRAY(tmpKey, 16);
    int keyLen = 16;
    memcpy(tmpKey, keys, keyLen);
    PedErr_t pinKeyErr = ped()->injectPinKey(tmpKey, keyLen);
    memset(tmpKey, 0x00, sizeof(tmpKey));
    memcpy(tmpKey, keys + keyLen, keyLen);
    PedErr_t macKeyErr = ped()->injectMacKey(tmpKey, keyLen);
    memset(tmpKey, 0x00, sizeof(tmpKey));
    memcpy(tmpKey, keys + (2 * keyLen), keyLen);
    PedErr_t dataKeyErr = ped()->injectDataKey(tmpKey, keyLen);
    RETURN_VALUE_IF_NOT(pinKeyErr, PED_ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(macKeyErr, PED_ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(dataKeyErr, PED_ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static inline Error_t setBit48() {
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    LOG_DEBUG("dn = %s", sn);
    DEFINE_STRING(privateData, 128);
    setCommonLtv(sn, PNA_APP_VERSION, 0 /*language*/, privateData);
    iso8583()->setStr(ELEMENT_ADDITIONAL_DATA_PRIVATE,
                      (const DL_UINT8*)privateData);
    return ERR_OK;
}

static void decodeBalanceValue(char* str, char* out, size_t len) {
    int  index          = 0;
    char accountType[3] = {0};
    char balanceType[3] = {0};
    char currency[5]    = {0};
    char debitType[2]   = {0};
    char balance[13]    = {0};

    LOG_TRACE("decodeBalanceValue::str[%s]", str);

    memcpy(accountType, str + index, 2);
    index += 2;
    memcpy(balanceType, str + index, 2);
    index += 2;
    memcpy(currency, str + index, 3);
    index += 3;
    memcpy(debitType, str + index, 1);
    index += 1;
    memcpy(balance, str + index, 12);
    index += 12;
    memset(out, 0, len);
    removeLeadingZeros(balance, out, len);
}

static Error_t checkIinData() {
    DEFINE_BYTE_ARRAY(feild, 128);
    size_t      size = sizeof(feild);
    IsoStatus_t ret =
        iso8583()->getBin(ELEMENT_ACQUIRING_INSTITUTION_ID, feild, &size);
    RETURN_VALUE_IF_NOT(ret, ISO_OK, ;, ERR_NOK);
    memset(settings()->terminal.acquirerIIN, 0,
           sizeof(settings()->terminal.acquirerIIN));
    memcpy(settings()->terminal.acquirerIIN, feild, size);
    return ERR_OK;
}

static Error_t setIIN() {
    if (strlen(settings()->terminal.acquirerIIN) != 0) {
        iso8583()->setStr(ELEMENT_ACQUIRING_INSTITUTION_ID,
                          (const DL_UINT8*)settings()->terminal.acquirerIIN);
    } else {
        iso8583()->setStr(ELEMENT_ACQUIRING_INSTITUTION_ID,
                          (const DL_UINT8*)"000000000");
    }
    return ERR_OK;
}

/*********************************************************************************************
 *                                                                                           *
 *                                   Builder/Parsers
 *                                                                                           *
 ********************************************************************************************/

Error_t isoBuildLogOn(TxnCore* txn, ByteArray* buf) {
    setBit48();
    /* set ISO message fields */
#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(32, (const DL_UINT8*)setting.AcquireIIN,
                                        &isoMsg); // IIN
#endif
    setSecRelCtrlInfo();
    return ERR_OK;
}

static Error_t isoParseLogOnResponse(ByteArray* buf) {
    /*
     MASTER		917862ab54de7c916091b5c9a7de0189
     PIN		d15d58bb29135b15f56025b2accc027f
     MAC		f876114e0cf7c40dbeaf75b785accf0c
     DATA		498de4bc8b32a43369ebdbaab20b8e0b
    */
#if defined CHECK_LOGON_MAC
    result = compareMac(&isoMsg, packBufTemp, TAK_INDEX, packedSize);
    if (result != SDK_OK)
        return result;
#endif
    DEFINE_STRING(feild, 1028);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild);
    memset(feild, 0, sizeof(feild));
    iso8583()->getStr(ELEMENT_TERMINAL_ID, feild);
    size_t terminalNumSize = sizeof(settings()->terminal.terminalId);
    memset(settings()->terminal.terminalId, 0, terminalNumSize);
    snprintf(settings()->terminal.terminalId, terminalNumSize - 1, "%s", feild);
    LOG_DEBUG("terminal number = %s", feild);
    RETURN_VALUE_IF_NOT(setWorkingKeys(), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

Error_t isoBuildCfg(TxnCore* txn, ByteArray* buf) {
    /* set ISO message fields */
#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(32, (const DL_UINT8*)setting.AcquireIIN,
                                        &isoMsg); // IIN
#endif
    setTerminalNum();
    setBit48();
    setSecRelCtrlInfo();
    return ERR_OK;
}

static Error_t isoParseCfgResponse(ByteArray* buf) {
    /*
     MASTER		917862ab54de7c916091b5c9a7de0189
     PIN		d15d58bb29135b15f56025b2accc027f
     MAC		f876114e0cf7c40dbeaf75b785accf0c
     DATA		498de4bc8b32a43369ebdbaab20b8e0b
    */
#if defined CHECK_LOGON_MAC
    result = compareMac(&isoMsg, packBufTemp, TAK_INDEX, packedSize);
    if (result != SDK_OK)
        return result;
#endif
    DEFINE_STRING(feild, 1028);
    size_t length;
    checkIinData();
    RESET_STRING(feild);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild);
    // compareMac(TAK_INDEX, buf);
    settings()->terminal.isCfgDone = true;
    return ERR_OK;
}

static Error_t isoBuildSettle(TxnCore* txn, ByteArray* buf) {
    DEFINE_STRING(amountstr, SIZE_AMOUNT + 1);
    prependZerosUInt64(txn->amount, SIZE_AMOUNT, amountstr, sizeof(amountstr));
    iso8583()->setStr(ELEMENT_AMOUNT_TRANSACTION, amountstr);

#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(
        32, (const ISO_UINT8*)setting.AcquireIIN, &isoMsg); // IIN
#else
    char* acquirerIin = strlen(settings()->terminal.acquirerIIN) != 0
                            ? settings()->terminal.acquirerIIN
                            : "000000000";
    iso8583()->setStr(ELEMENT_ACQUIRING_INSTITUTION_ID, acquirerIin);
#endif
    iso8583()->setStr(ELEMENT_RETRIEVAL_REFERENCE_NUMBER, txn->rrn);
    iso8583()->setStr(ELEMENT_TERMINAL_ID, settings()->terminal.terminalId);
    iso8583()->setStr(ELEMENT_CARD_ACCEPTOR_ID,
                      settings()->terminal.merchantUniqueId);
    iso8583()->setStr(ELEMENT_CURRENCY_CODE_TRANSACTION, "364");
    setSecRelCtrlInfo();
    return ERR_OK;
}

static Error_t isoParseSettle(ByteArray* buf) {
    // check mac
}

static Error_t isoBuildReverse(TxnCore* txn, ByteArray* buf) {
    DEFINE_STRING(amountstr, SIZE_AMOUNT + 1);
    prependZerosUInt64(txn->amount, SIZE_AMOUNT, amountstr, sizeof(amountstr));
    iso8583()->setStr(ELEMENT_AMOUNT_TRANSACTION, amountstr);

#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(
        32, (const ISO_UINT8*)setting.AcquireIIN, &isoMsg); // IIN
#else
    char* acquirerIin = strlen(settings()->terminal.acquirerIIN) != 0
                            ? settings()->terminal.acquirerIIN
                            : "000000000";
    iso8583()->setStr(ELEMENT_ACQUIRING_INSTITUTION_ID, acquirerIin);
#endif
    iso8583()->setStr(ELEMENT_RETRIEVAL_REFERENCE_NUMBER, txn->rrn);
    iso8583()->setStr(ELEMENT_TERMINAL_ID, settings()->terminal.terminalId);
    iso8583()->setStr(ELEMENT_CARD_ACCEPTOR_ID,
                      settings()->terminal.merchantUniqueId);
    iso8583()->setStr(ELEMENT_CURRENCY_CODE_TRANSACTION, "364");
    setSecRelCtrlInfo();
    iso8583()->setStr(ELEMENT_ORIGINAL_DATA_ELEMENTS, "");
    return ERR_OK;
}

static Error_t isoParseReverse(ByteArray* buf) {
    // check mac
}

static Error_t isoBuildPurchase(TxnCore* txn, ByteArray* buf) {}

static Error_t isoParsePurchaseResponse(ByteArray* buf) {}

static Error_t isoBuildBill(TxnCore* txn, ByteArray* buf) {}

static Error_t isoParseBillResponse(ByteArray* buf) {}

static Error_t isoBuildBalance(TxnCore* txn, ByteArray* buf) {
    DEFINE_STRING(pan, 24);
    DEFINE_STRING(pinblock, 24);
    magreader()->getPan(pan, sizeof(pan));
    LOG_DEBUG("pan = %s", pan);
    TrackData_t track2 = OOP_CALL(magreader(), getTrack2);
    LOG_DEBUG("track2 = %s", track2.data);
    PedErr_t pederr =
        OOP_CALL(ped(), getPinBlock, pan, pinblock, sizeof(pinblock));
    RETURN_VALUE_IF_NOT(pederr, PED_ERR_OK, ;, ERR_NOK);
    iso8583()->setStr(ELEMENT_PAN, pan);
    iso8583()->setStr(ELEMENT_POS_ENTRY_MODE, "021");
    iso8583()->setStr(ELEMENT_POS_CONDITION_CODE, "14");
    setIIN();
    iso8583()->setStr(ELEMENT_TRACK2, track2.data);
    iso8583()->setStr(ELEMENT_TERMINAL_ID, settings()->terminal.terminalId);
    iso8583()->setStr(ELEMENT_CARD_ACCEPTOR_ID,
                      settings()->terminal.merchantUniqueId);
    setBit48();
    iso8583()->setBin(ELEMENT_PIN_DATA, pinblock, PIN_BLOCK_LEN);
    setSecRelCtrlInfo();
    return ERR_OK;
}

static Error_t isoParseBalanceResponse(ByteArray* buf) {
    DEFINE_STRING(feild, 1028);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild);
    RESET_STRING(feild);
    DEFINE_STRING(balance, 16);
    DEFINE_STRING(ledger, 16);
    if (iso8583()->getStr(ELEMENT_ADDITIONAL_AMOUNTS, feild) == ISO_OK) {
        decodeBalanceValue(feild, balance, sizeof(balance));
        decodeBalanceValue(feild + 20, ledger, sizeof(ledger));
        LOG_TRACE("Parser: balance txn amount is in Bit 54. balance = %s, "
                  "ledger = %s",
                  balance, ledger);
    } else if (iso8583()->getStr(ELEMENT_AMOUNT_TRANSACTION, feild) == ISO_OK) {
        removeLeadingZeros(feild, balance, sizeof(balance));
        LOG_TRACE("Parser: balance txn amount is in Bit 4. balance = %s",
                  balance);
    }
    return ERR_OK;
}

static Error_t isoBuildPay(TxnCore* txn, ByteArray* buf) {}

static Error_t isoParsePayResponse(ByteArray* buf) {}

/*********************************************************************************************
 *                                                                                           *
 *                                      Common
 *                                                                                           *
 ********************************************************************************************/

static Error_t isoBuildMac(Mti_t mti, ByteArray* buf) {
    DEFINE_BYTE_ARRAY(mac, ISO_MAC_LEN + 1);
    DEFINE_BYTE_ARRAY(macHex, ISO_MAC_LEN + 1);
    uint8_t field = mti == MTI_REV_ADVICE ? ELEMENT_MAC_2 : ELEMENT_MAC;
    iso8583()->setBin(field, (const DL_UINT8*)mac, ISO_MAC_LEN);
    DEFINE_BYTE_ARRAY(tmpBuf, ISO_MAX_BUFFER);
    size_t packedLen;
    RETURN_VALUE_IF_NOT(iso8583()->pack(tmpBuf, &packedLen), ISO_OK, ;
                        , ERR_NOK);
    RETURN_VALUE_IF_NOT((packedLen < ISO_MAC_LEN), false, ;, ERR_NOK);
    LOG_TRACE("ISO Build mac: packed len = %u", packedLen);
    PedErr_t pedErr = ped()->getMac(16, tmpBuf, packedLen - ISO_MAC_LEN, mac);
    RETURN_VALUE_IF_NOT(pedErr, PED_ERR_OK, ;, ERR_NOK);
    bytesToHex(mac, 4, macHex, sizeof(macHex));
    LOG_TRACE("ISO Build mac: mac hex = %s", macHex);
    iso8583()->setBin(field, (const DL_UINT8*)macHex, ISO_MAC_LEN);
    return ERR_OK;
}

static Error_t isoPack(ByteArray* buf) {
    DEFINE_BYTE_ARRAY(tmpBuf, ISO_MAX_BUFFER);
    size_t packedLen;
    RETURN_VALUE_IF_NOT(iso8583()->pack(tmpBuf, &packedLen), ISO_OK, ;
                        , ERR_NOK);
    IsoHeaderData_t hd = {.nii = settings()->server.mainServerNii};
    RETURN_VALUE_IF_NOT(
        iso8583()->addHeader(buf->data, tmpBuf, &packedLen, &hd), ISO_OK, ;
        , ERR_NOK);
    buf->len = packedLen;
    return ERR_OK;
}

Error_t isoBuild(Mti_t mti, PrCode_t prcode, TxnCore* txn, ByteArray* buf) {
    IsoTransaction* itxn = isoFindTransaction(mti, prcode);
    RETURN_VALUE_IF_NULL(itxn, ;, ERR_NOK);
    iso8583()->reset();
    setMti(mti);
    if (mti != MTI_FIN_ADVICE && mti != MTI_REV_ADVICE) {
        setPrCode(itxn->prcode);
    } else {
        setPrCode(txn->processCode);
    }
    setStan(txnTraceInfo()->stan);
    setDateTime();
    setNii();
    RETURN_VALUE_IF_NOT(itxn->builder(txn, buf), ERR_OK, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(isoBuildMac(mti, buf), ERR_OK, ;, ERR_NOK);
    return isoPack(buf);
}

static Error_t checkMac(const char* data, size_t len) {
    DEFINE_BYTE_ARRAY(isoMac, 128);
    DEFINE_BYTE_ARRAY(mac, ISO_MAC_LEN);
    ped()->getMac(16, data, len, mac);
    bool   isMacRetrived = false;
    size_t size          = sizeof(isoMac);
    if (iso8583()->getBin(ELEMENT_MAC_2, isoMac, &size) == ISO_OK) {
        isMacRetrived = true;
        LOG_TRACE("Parser: mac is availabe in Bit 128.");
    } else if (iso8583()->getBin(ELEMENT_MAC, isoMac, &size) == ISO_OK) {
        isMacRetrived = true;
        LOG_TRACE("Parser: mac is availabe in Bit 64.");
    }
    RETURN_VALUE_IF_NOT(isMacRetrived, true, ;, ERR_NOK);
    int res = memcmp(isoMac, mac, ISO_MAC_LEN);
    RETURN_VALUE_IF_NOT(res, 0, ;, ERR_NOK);
    return ERR_OK;
}

RespCode_t isoParse(Mti_t mti, PrCode_t prcode, ByteArray* buf) {
    RETURN_VALUE_IF_NULL(buf, ;, ERR_NULL_PARAMETER);
    IsoStatus_t st = iso8583()->parse(buf->data, buf->len);
    RETURN_VALUE_IF_NOT(st, ISO_OK, ;, ERR_NOK);
    // Check mac
    // uint16_t packedSize = buf->data[0] * 256 + buf->data[1];
    // packedSize          = packedSize - 5; // without header
    // Error_t res         = checkMac(buf->data + 7, packedSize - ISO_MAC_LEN);
    // RETURN_VALUE_IF_NOT(res, ERR_OK, ;, ERR_NOK);
    // Check responce code
    DEFINE_STRING(f39, 8);
    iso8583()->getStr(ELEMENT_RESPONSE_CODE, f39);
    RespCode_t respCode = libAtoi(f39);
    LOG_TRACE("Parser: txn responce code = %d", respCode);
    RETURN_VALUE_IF_NOT(respCode, 0, ;, respCode);

    IsoTransaction* txn = isoFindTransaction(mti, prcode);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(txn->parser(buf), ERR_OK, ;, ERR_NOK);
    return respCode;
}

static const IsoTransaction templates[] = {
    /******************************************************************/
    /*LOG ON*/
    /******************************************************************/
    {.mti     = MTI_NET_REQ,
     .prcode  = PRC_LOG_ON,
     .builder = isoBuildLogOn,
     .parser  = isoParseLogOnResponse},
    /******************************************************************/
    /*Configuration*/
    /******************************************************************/
    {.mti     = MTI_AUTH_REQ,
     .prcode  = PRC_CFG,
     .builder = isoBuildCfg,
     .parser  = isoParseCfgResponse},
    /******************************************************************/
    /*Settle*/
    /******************************************************************/
    {.mti     = MTI_FIN_ADVICE,
     .prcode  = PRC_SETTLE,
     .builder = isoBuildSettle,
     .parser  = isoParseSettle},
    /******************************************************************/
    /*Reverse*/
    /******************************************************************/
    {.mti     = MTI_REV_ADVICE,
     .builder = isoBuildReverse,
     .parser  = isoParseReverse,
     .prcode  = PRC_REVERSE},
    /******************************************************************/
    /*Purchase*/
    /******************************************************************/
    {.mti     = MTI_FIN_REQ,
     .prcode  = PRC_PURCHASE,
     .builder = isoBuildPurchase,
     .parser  = isoParsePurchaseResponse},
    /******************************************************************/
    /*Bill Payment*/
    /******************************************************************/
    {.mti     = MTI_FIN_REQ,
     .prcode  = PRC_BILL_PAYMENT,
     .builder = isoBuildBill,
     .parser  = isoParseBillResponse},
    /******************************************************************/
    /*Balance Inquiry*/
    /******************************************************************/
    {.mti     = MTI_AUTH_REQ,
     .prcode  = PRC_BALANCE,
     .builder = isoBuildBalance,
     .parser  = isoParseBalanceResponse}};

const IsoTransaction* isoFindTransaction(Mti_t mti, PrCode_t prcode) {
    for (size_t i = 0; i < ARRAY_SIZE(templates); i++) {
        if (templates[i].mti == mti && templates[i].prcode == prcode) {
            return &templates[i];
        }
    }
    return NULL;
}