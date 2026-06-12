#include "isoBuilder.h"
#include "sys/sys.h"
#include "settings/settings.h"
#include "ped/ped.h"
#include "utility/ltv.h"
#include "utility/utility.h"
#include "utility/arith.h"


#define MTI_VAL_LOG_ON              "0800"
#define MTI_VAL_CFG                 "0100"

#define PRC_LOG_ON              "920000"
#define PRC_CFG                 "930000"

#define STAN_SIZE   6

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
 *                                      Helpers                                              * 
 *                                                                                           *
 ********************************************************************************************/

static int parse2Digits(const unsigned char *p) {
    char buf[3] = {
        (char)p[0],
        (char)p[1],
        '\0'
    };

    return libAtoi(buf);
}

static int8_t compareMac(int keyIndex, char* data) {
    RETURN_VALUE_IF_NULL(data, ;, ERR_NULL_PARAMETER);
    uint16_t packedSize = data[0] * 256 + data[1];
	packedSize = packedSize - 5;				// without header
    DL_UINT8 packBufTemp[2048];
    memset(packBufTemp, 0x00, sizeof(packBufTemp));
	memcpy(packBufTemp, data + 7, packedSize);

    DEFINE_BYTE_ARRAY(mac, 9);
    DEFINE_BYTE_ARRAY(asciiMac, 9);
    size_t length;
    if (iso8583()->getBin(ELEMENT_MAC_2, mac, &length) == ISO_OK) {
    } else if (iso8583()->getBin(ELEMENT_MAC, mac, &length) == ISO_OK) {
    } else {
        LOG_DEBUG("mac:::::::::NO MAC !!!!");
        return ERR_NOK;
    }
    LOG_DEBUG("[mac (%d)] [%s]", length, mac);
    // pedGetMacAscii(keyIndex, 16, packBufTemp, packedSize - 8, (char *)asciiMac);
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

static int8_t pedDecrypt(void *buffer, int bufLen, void *decryptedData) {
    RETURN_VALUE_IF_NULL(buffer, ;, ERR_NULL_PARAMETER);
    RETURN_VALUE_IF_NULL(decryptedData, ;, ERR_NULL_PARAMETER);
	PedErr_t result = OOP_CALL(ped(), encryptAccountData, buffer, bufLen, decryptedData);
	return result == PED_ERR_OK? ERR_OK : ERR_NOK;
}

static void handleMerchantName(LtvStructInfo *tag) {
    DEFINE_BYTE_ARRAY(tmpBuffer, 512);

    memset(settings()->terminal.merchantName, 0,
           sizeof(settings()->terminal.merchantName));

    hexStringToBytes(tag->data, tag->len - 1, tmpBuffer);

    char *end = strchr((char *)tmpBuffer, '\\');
    size_t len = end
               ? (size_t)(end - (char *)tmpBuffer)
               : strlen((char *)tmpBuffer);

    LOG_DEBUG("merchantFa (%zu): %.*s",
              len,
              (int)len,
              tmpBuffer);

    convertStrNoNumber(
        (char *)tmpBuffer,
        (int)len, 0, 4, 5,
        settings()->terminal.merchantName);

    LOG_DEBUG(
        "merchantName (%zu): %s",
        strlen(settings()->terminal.merchantName),
        settings()->terminal.merchantName);
}

static void handleMerchantPhone(LtvStructInfo *tag) {
    memset(settings()->terminal.merchantPhone, 0x00, 
            sizeof(settings()->terminal.merchantPhone));
	hexStringToBytes(tag->data, tag->len - 1, 
            (unsigned char *)settings()->terminal.merchantPhone);
	// strcpy (config.MerchantPhone, ltvStructInfo[i].data);
	LOG_DEBUG(" config.MerchantPhone [%s]", settings()->terminal.merchantPhone);
}

static void handleSwitchDateTime(LtvStructInfo *tag) {
    int dateTime[6] = {0};
    unsigned char tempBuf[512] = {0};

    LOG_DEBUG("dateTime(%d) [%s]", tag->len, tag->data);

    hexStringToBytes(tag->data, tag->len - 1, tempBuf);

    for (int i = 0; i < 6; i++) {
        dateTime[i] = parse2Digits(tempBuf + 2 + (i * 2));
    }

    int ret = dtSetSystemDateTime(dateTime);

    LOG_DEBUG(
        "dtSetSystemDateTime(%s) -> %d",
        tempBuf,
        ret
    );
}

static void handleVoucherPin(LtvStructInfo *tag) {
    char temp[128] = {0};
	int pinLen = 0;
	unsigned char pinTemp[64];
	unsigned char pinBytes[32];
	unsigned char pinData[32];
	unsigned int outLen = 0;
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
	//TraceExt(pinTemp, 16, "VoucherPin [%s]", currentTransaction.sqlTrx.PaymentId);
}

static void handleMerchantUniqueId(LtvStructInfo *tag) {
    memset(settings()->terminal.merchantUniqueId, 0x00, sizeof(settings()->terminal.merchantUniqueId));
    hexStringToBytes(tag->data, tag->len - 1, (unsigned char *)settings()->terminal.merchantUniqueId);
    LOG_DEBUG("settings()->terminal.merchantUniqueId [%s]", 
            settings()->terminal.merchantUniqueId);
    // settings()->save();
}

static void decodeMerchantDesc(char *buffer) {
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

Error_t setDateTime() {
    DateTime *dt = OOP_CALL(sys(), getDateTime);
    iso8583()->setStr(ELEMENT_TIME_LOCAL_TRANSACTION, (const DL_UINT8 *)dt->time);
    iso8583()->setStr(ELEMENT_DATE_LOCAL_TRANSACTION, (const DL_UINT8 *)dt->date + 2);
    return ERR_OK;
}

void setStan() {
    // txnTraceInfo()->inc();
    DEFINE_STRING(stan, (STAN_SIZE + 1));
    prependZerosInt(txnTraceInfo()->stan, STAN_SIZE, stan, sizeof(stan));
    stan[STAN_SIZE] = 0;
    LOG_DEBUG("stan = %d, stan string = %s", txnTraceInfo()->stan, stan);
    iso8583()->setStr(ELEMENT_STAN, (const DL_UINT8 *)stan);
}

/*********************************************************************************************
 *                                                                                           *
 *                                   Builder/Parsers                                         * 
 *                                                                                           *
 ********************************************************************************************/

Error_t isoBuildLogOn(ByteArray *buf) {
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    LOG_DEBUG("dn = %s", sn );
    DEFINE_STRING(privateData, 128);
	setCommonLtv(sn, PNA_APP_VERSION, 0 /*language*/, privateData);
    /* set ISO message fields */
    iso8583()->setMTI((const DL_UINT8 *)MTI_VAL_LOG_ON);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8 *)PRC_LOG_ON);
    setStan();
    setDateTime();
    DEFINE_STRING(nni, 8);
    prependZerosInt(settings()->server.mainServerNii, 4, nni, sizeof(nni));
    iso8583()->setStr(ELEMENT_NETWORK_INTL_ID, (const DL_UINT8 *)nni);
#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(32, (const DL_UINT8 *)setting.AcquireIIN, &isoMsg); // IIN
#endif
    iso8583()->setStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, (const DL_UINT8 *)privateData);
    iso8583()->setStr(ELEMENT_SECURITY_CONTROL_INFO, (const DL_UINT8 *)SecRelControlInfo);
    return ERR_OK;
}

static Error_t isoParseLogOnResponse(ByteArray *buf) {
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
    DEFINE_STRING(f48, 1028);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, f48);
    DEFINE_STRING(f41, 256);
    iso8583()->getStr(ELEMENT_TERMINAL_ID, f41);
    LOG_DEBUG("terminal number = %s", f41);
	decodeMerchantDesc(f48);
    settings()->save();
	return ERR_OK;
}

Error_t isoBuildCfg(ByteArray *buf) {
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    LOG_DEBUG("dn = %s", sn );
    DEFINE_STRING(privateData, 128);
	setCommonLtv(sn, PNA_APP_VERSION, 0 /*language*/, privateData);
    /* set ISO message fields */
    iso8583()->setMTI((const DL_UINT8 *)MTI_VAL_CFG);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8 *)PRC_CFG);
    setStan();
    setDateTime();
    DEFINE_STRING(nni, 8);
    prependZerosInt(settings()->server.mainServerNii, 4, nni, sizeof(nni));
    iso8583()->setStr(ELEMENT_NETWORK_INTL_ID, (const DL_UINT8 *)nni);
#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(32, (const DL_UINT8 *)setting.AcquireIIN, &isoMsg); // IIN
#endif
    iso8583()->setStr(ELEMENT_TERMINAL_ID, (const DL_UINT8 *)privateData);
    iso8583()->setStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, 
                        (const DL_UINT8 *)settings()->terminal.terminalNo);
    return ERR_OK;
}

static Error_t isoParseCfgResponse(ByteArray *buf) {
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
    DEFINE_STRING(acquirerIIN, 11);
    size_t length;
    iso8583()->getBin(ELEMENT_ACQUIRING_INSTITUTION_ID, acquirerIIN, &length);
    LOG_DEBUG("[[ acquirerIIN (%d)(%s) ]]", length, acquirerIIN);
    memcpy(settings()->terminal.acquirerIIN, acquirerIIN, length);
    settings()->terminal.acquirerIIN[length] = '\0';  
    
    LOG_DEBUG("[[ setting.AcquireIIN (%d)(%s) ]]", 
                strlen(settings()->terminal.acquirerIIN),
                    settings()->terminal.acquirerIIN);
    DEFINE_STRING(f48, 16);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, f48);
	decodeMerchantDesc(f48);
    // compareMac(TAK_INDEX, buf);
    settings()->save();
	return ERR_OK;
}

static Error_t isoBuildPurchase(ByteArray *buf) {
}

static Error_t isoParsePurchaseResponse(ByteArray *buf) {
}

static Error_t isoBuildBill(ByteArray *buf) {
}

static Error_t isoParseBillResponse(ByteArray *buf) {
}

static Error_t isoBuildBalance(ByteArray *buf) {
    
}

static Error_t isoParseBalanceResponse(ByteArray *buf) {
}

static Error_t isoBuildPay(ByteArray *buf) {
}

static Error_t isoParsePayResponse(ByteArray *buf) {
}

static Error_t isoBuildMac(ByteArray *buf) {
    DEFINE_BYTE_ARRAY(mac, 8 + 1);
    iso8583()->setBin(ELEMENT_MAC, (const DL_UINT8 *)mac, 8);
    DEFINE_BYTE_ARRAY(tmpBuf, ISO_MAX_BUFFER);
    size_t packedLen;
    RETURN_VALUE_IF_NOT(iso8583()->pack(tmpBuf, &packedLen), ISO_OK, ;, ERR_NOK);
    if (packedLen < 8)
        return ERR_NOK;
    ped()->getMac(16, tmpBuf, packedLen - 8, mac);
    iso8583()->setBin(ELEMENT_MAC, (const DL_UINT8 *)mac, 8);
    RETURN_VALUE_IF_NOT(iso8583()->pack(tmpBuf, &packedLen), ISO_OK, ;, ERR_NOK);
    IsoHeaderData_t hd = {
        .nii = settings()->server.mainServerNii
    };
    RETURN_VALUE_IF_NOT(iso8583()->addHeader(buf->data,
                            tmpBuf, &packedLen, &hd), 
                            ISO_OK, ;, ERR_NOK);
    buf->len = packedLen;
    return ERR_OK;
}

Error_t isoBuild(MTI_t mti, ByteArray *buf) {
    IsoTransaction *txn = isoFindTransaction(mti);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_NOK);
    iso8583()->reset();
    RETURN_VALUE_IF_NOT(txn->builder(buf), ERR_OK, ;, ERR_NOK);
    return isoBuildMac(buf);
}

RespCode_t isoParse(MTI_t mti, ByteArray *buf) {
    RETURN_VALUE_IF_NULL(buf, ;, ERR_NULL_PARAMETER);
    IsoStatus_t st = iso8583()->parse(buf->data, buf->len);
    RETURN_VALUE_IF_NOT(st, ISO_OK, ;, ERR_NOK);
    // Check responce code
    DEFINE_STRING(f39, 8);
    iso8583()->getStr(ELEMENT_RESPONSE_CODE, f39);
    LOG_DEBUG("f39 = %s", f39);
    RespCode_t respCode = libAtoi(f39);
    LOG_DEBUG("Txn responce code = %d", respCode);
    RETURN_VALUE_IF_NOT(respCode , 0, ;, respCode);

    IsoTransaction *txn = isoFindTransaction(mti);
    RETURN_VALUE_IF_NULL(txn, ;, ERR_NOK);
        LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    RETURN_VALUE_IF_NOT(txn->parser(buf), ERR_OK, ;, ERR_NOK);
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    LOG_DEBUG("=========================");
    
    return respCode;
}

static const IsoTransaction templates[] = {
    {
        .mti         = MTI_LOG_ON,
        .builder     = isoBuildLogOn,
        .parser      = isoParseLogOnResponse
    },
    {
        .mti         = MTI_CFG,
        .builder     = isoBuildCfg,
        .parser      = isoParseCfgResponse
    },
    {
        .mti         = MTI_PURCHASE,
        .builder     = isoBuildPurchase,
        .parser      = isoParsePurchaseResponse
    },
    {
        .mti         = MTI_BILL,
        .builder     = isoBuildBill,
        .parser      = isoParseBillResponse
    },
    {
        .mti         = MTI_BALANCE,
        .builder     = isoBuildBalance,
        .parser      = isoParseBalanceResponse
    },
    {
        .mti         = MTI_PAY,
        .builder     = isoBuildPay,
        .parser      = isoParsePayResponse
    }
};

const IsoTransaction *isoFindTransaction(MTI_t mti) {
    for (size_t i = 0; i < ARRAY_SIZE(templates); i++) {
        if (templates[i].mti == mti) {
            return &templates[i];
        }
    }
    return NULL;
}