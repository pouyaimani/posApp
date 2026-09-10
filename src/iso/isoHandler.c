#include "isoHandler.h"
#include "sys/sys.h"
#include "settings/settings.h"
#include "ped/ped.h"
#include "utility/ltv.h"
#include "utility/utility.h"
#include "utility/arith.h"
#include "magReader/magReader.h"
#include "isoHelper.h"

/*********************************************************************************************
 *                                                                                           *
 *                                      Helpers
 *                                                                                           *
 ********************************************************************************************/

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
        LOG_TRACE("mac:::::::::NO MAC !!!!");
        return ERR_NOK;
    }
    LOG_TRACE("[mac (%d)] [%s]", length, mac);
    // pedGetMacAscii(keyIndex, 16, packBufTemp, packedSize - 8, (char
    // *)asciiMac);
    int8_t macCompare = memcmp(mac, asciiMac, 8);
    if (macCompare != 0) {
        LOG_TRACE("mac:::::::::check mac failed !!!!");
        return ERR_NOK;
    }
    LOG_TRACE("mac:::::::::check mac succeed !!!!");
    return ERR_OK;
}

static Error_t setWorkingKeys() {
    DEFINE_BYTE_ARRAY(keys, 512);
    size_t      size = sizeof(keys);
    IsoStatus_t ret =
        iso8583()->getBin(ELEMENT_RESERVED_PRIVATE_62, keys, &size);
    RETURN_VALUE_IF_NOT(ret, ISO_OK, ;, ERR_NOK);
    LOG_TRACE("Keys length = %d, keys = %s", size, keys);
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

/*********************************************************************************************
 *                                                                                           *
 *                                   Builder/Parsers
 *                                                                                           *
 ********************************************************************************************/

static Error_t isoParseLogOnResponse(TxnData* txn, ByteArray* buf) {
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
    decodeMerchantDesc(feild, txn);
    memset(feild, 0, sizeof(feild));
    iso8583()->getStr(ELEMENT_TERMINAL_ID, feild);
    size_t terminalNumSize = sizeof(settings()->terminal.terminalId);
    memset(settings()->terminal.terminalId, 0, terminalNumSize);
    snprintf(settings()->terminal.terminalId, terminalNumSize - 1, "%s", feild);
    LOG_DEBUG("terminal number = %s", feild);
    RETURN_VALUE_IF_NOT(setWorkingKeys(), ERR_OK, ;, ERR_NOK);
    return ERR_OK;
}

static Error_t isoParseCfgResponse(TxnData* txn, ByteArray* buf) {
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
    iso8583()->getStr(ELEMENT_CARD_ACCEPTOR_ID, feild);
    RESET_STRING(settings()->terminal.merchantId);
    strcpy(settings()->terminal.merchantId, feild);
    LOG_TRACE("Merchant Id = %s", settings()->terminal.merchantId);
    RESET_STRING(feild);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild, txn);
    // compareMac(TAK_INDEX, buf);
    settings()->terminal.isCfgDone = true;
    return ERR_OK;
}

static Error_t isoParseDefault(TxnData* txn, ByteArray* buf) { return ERR_OK; }

static Error_t isoParseBillResponse(TxnData* txn, ByteArray* buf) {}

static Error_t isoParseBalanceResponse(TxnData* txn, ByteArray* buf) {
    DEFINE_STRING(feild, 1028);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild, txn);
    RESET_STRING(feild);
    DEFINE_STRING(available, 16);
    DEFINE_STRING(ledger, 16);
    if (iso8583()->getStr(ELEMENT_ADDITIONAL_AMOUNTS, feild) == ISO_OK) {
        decodeBalanceValue(feild, available, sizeof(available));
        decodeBalanceValue(feild + 20, ledger, sizeof(ledger));
        LOG_TRACE("Parser: balance txn amount is in Bit 54. available = %s, "
                  "ledger = %s",
                  available, ledger);
    } else if (iso8583()->getStr(ELEMENT_AMOUNT_TRANSACTION, feild) == ISO_OK) {
        removeLeadingZeros(feild, available, sizeof(available));
        LOG_TRACE("Parser: balance txn amount is in Bit 4. available = %s",
                  available);
    }
    str2u64(available, &txn->extention.balance.available);
    str2u64(ledger, &txn->extention.balance.ledger);
    return ERR_OK;
}

static Error_t isoParsePayResponse(TxnData* txn, ByteArray* buf) {}

static Error_t isoParseVoucherResponse(TxnData* txn, ByteArray* buf) {
    DEFINE_STRING(feild, 1028);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, feild);
    decodeMerchantDesc(feild, txn);
    return ERR_OK;
}

/*********************************************************************************************
 *                                                                                           *
 *                                      Common
 *                                                                                           *
 ********************************************************************************************/

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

static Error_t checkMac(const char* data, size_t len) {
    DEFINE_BYTE_ARRAY(isoMac, 128);
    DEFINE_BYTE_ARRAY(mac, LEN_MAX_ISO_MAC);
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
    int res = memcmp(isoMac, mac, LEN_MAX_ISO_MAC);
    RETURN_VALUE_IF_NOT(res, 0, ;, ERR_NOK);
    return ERR_OK;
}

static const IsoTransaction templates[] = {
    /******************************************************************/
    /*LOG ON*/
    /******************************************************************/
    {.mti     = MTI_NET_REQ,
     .prcode  = PRC_LOG_ON,
     .builder = NULL,
     .parser  = isoParseLogOnResponse},
    /******************************************************************/
    /*Configuration*/
    /******************************************************************/
    {.mti     = MTI_AUTH_REQ,
     .prcode  = PRC_CFG,
     .builder = NULL,
     .parser  = isoParseCfgResponse},
    /******************************************************************/
    /*Settle*/
    /******************************************************************/
    {.mti     = MTI_FIN_ADVICE,
     .prcode  = PRC_SETTLE,
     .builder = NULL,
     .parser  = isoParseDefault},
    /******************************************************************/
    /*Reverse*/
    /******************************************************************/
    {.mti     = MTI_REV_ADVICE,
     .builder = NULL,
     .parser  = isoParseDefault,
     .prcode  = PRC_REVERSE},
    /******************************************************************/
    /*Purchase*/
    /******************************************************************/
    {.mti     = MTI_FIN_REQ,
     .prcode  = PRC_PURCHASE,
     .builder = NULL,
     .parser  = isoParseDefault},
    /******************************************************************/
    /*Bill Payment*/
    /******************************************************************/
    {.mti     = MTI_FIN_REQ,
     .prcode  = PRC_BILL_PAYMENT,
     .builder = NULL,
     .parser  = isoParseBillResponse},
    /******************************************************************/
    /*Topup*/
    /******************************************************************/
    {.mti     = MTI_FIN_REQ,
     .prcode  = PRC_TOPUP,
     .builder = NULL,
     .parser  = isoParseDefault},
    /******************************************************************/
    /*Voucher*/
    /******************************************************************/
    {.mti     = MTI_FIN_REQ,
     .prcode  = PRC_VOUCHER,
     .builder = NULL,
     .parser  = isoParseVoucherResponse},
    /******************************************************************/
    /*Balance Inquiry*/
    /******************************************************************/
    {.mti     = MTI_AUTH_REQ,
     .prcode  = PRC_BALANCE,
     .builder = NULL,
     .parser  = isoParseBalanceResponse}};

const IsoTransaction* isoFindTransaction(Mti_t mti, PrCode_t prcode) {
    for (size_t i = 0; i < ARRAY_SIZE(templates); i++) {
        if (templates[i].mti == mti && templates[i].prcode == prcode) {
            return &templates[i];
        }
    }
    return NULL;
}

Error_t isoBuild(Mti_t mti, PrCode_t prcode, uint8_t* feild, uint16_t feildsCnt,
                 TxnData* txn, ByteArray* buf) {
    LOG_TRACE("ISO Buider: mti = %u, process code = %u", mti, prcode);
    IsoTransaction* itxn = isoFindTransaction(mti, prcode);
    RETURN_VALUE_IF_NULL(itxn, ;, ERR_NOK);
    iso8583()->reset();
    setMti(mti);
    LOG_TRACE("ISO Buider: feilds count = %u", feildsCnt);
    for (size_t i = 0; i < feildsCnt; i++) {
        for (size_t j = 0; j < sizeof(isoHelper); j++) {
            if (isoHelper[j].feild == feild[i]) {
                if (isoHelper[j].set) {
                    isoHelper[j].set(txn);
                    break;
                }
            }
        }
    }
    if (itxn->builder) {
        RETURN_VALUE_IF_NOT(itxn->builder(txn, buf), ERR_OK, ;, ERR_NOK);
    }
    return isoPack(buf);
}

RespCode_t isoParse(Mti_t mti, PrCode_t prcode, TxnData* txn, ByteArray* buf) {
    RETURN_VALUE_IF_NULL(buf, ;, ERR_NULL_PARAMETER);
    IsoStatus_t st = iso8583()->parse(buf->data, buf->len);
    RETURN_VALUE_IF_NOT(st, ISO_OK, ;, ERR_NOK);

    // Check mac
    // uint16_t packedSize = buf->data[0] * 256 + buf->data[1];
    // packedSize          = packedSize - 5; // without header
    // Error_t res         = checkMac(buf->data + 7, packedSize -
    // LEN_MAX_ISO_MAC);
    // RETURN_VALUE_IF_NOT(res, ERR_OK, ;, ERR_NOK);

    DEFINE_STRING(feild, 128);
    iso8583()->getStr(ELEMENT_RESPONSE_CODE, feild);
    RespCode_t respCode;
    STRING_TO_U16(feild, (uint8_t*)&respCode);
    txn->core.respCode = respCode;
    LOG_TRACE("Parser: txn responce code = %d", respCode);
    RESET_STRING(feild);
    txn->core.rrn = txn->core.trace = 0;
    if (iso8583()->getStr(ELEMENT_RETRIEVAL_REFERENCE_NUMBER, feild) ==
        ISO_OK) {
        if (!STRING_TO_U64(feild, &txn->core.rrn)) {
            LOG_ERROR("Iso Parser: error in converting rrn to llu");
        }
    }
    RESET_STRING(feild);
    if (iso8583()->getStr(ELEMENT_AUTH_ID_RESPONSE, feild) == ISO_OK) {
        STRING_TO_U32(feild, &txn->core.trace);
    }
    LOG_TRACE("Iso Parser: txn rrn = %llu", txn->core.rrn);
    LOG_TRACE("Iso Parser: txn trace = %lu", txn->core.trace);
    RETURN_VALUE_IF_NOT(respCode, RESP_CODE_SUCESS, ;, respCode);
    // for (int i = 0; i < iso8583()->handler.fieldItems; i++) {
    //     if (iso8583()->msg.field[i].ptr != NULL) {
    //         for (size_t j = 0; j < sizeof(isoHelper); j++) {
    //             if (isoHelper[j].feild == i) {
    //                 if (isoHelper[j].get) {
    //                     isoHelper[j].get(txn);
    //                 }
    //                 break;
    //             }
    //         }
    //     }
    // }
    IsoTransaction* itxn = isoFindTransaction(mti, prcode);
    RETURN_VALUE_IF_NULL(itxn, ;, ERR_NOK);
    RETURN_VALUE_IF_NOT(itxn->parser(txn, buf), ERR_OK, ;, ERR_NOK);
    return respCode;
}