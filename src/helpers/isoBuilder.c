#include "isoBuilder.h"
#include "sys/sys.h"
#include "settings/settings.h"
#include "ped/ped.h"
#include "utility/ltv.h"
#include "utility/utility.h"

#define MTI_VAL_LOG_ON              "0800"
#define MTI_VAL_CFG                 "0100"

#define PRC_LOG_ON              "920000"
#define PRC_CFG                 "930000"

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
        LOG_DEBUG("+++++++++++++mac:::::::::NO MAC !!!!");
        return ERR_NOK;
    }
    LOG_DEBUG("[mac (%d)] [%s]", length, mac);
    // pedGetMacAscii(keyIndex, 16, packBufTemp, packedSize - 8, (char *)asciiMac);
	int8_t macCompare = memcmp(mac, asciiMac, 8);
    if (macCompare != 0) {
		LOG_DEBUG("+++++++++++++mac:::::::::check mac failed !!!!");
		return ERR_NOK;
	}
	LOG_DEBUG("+++++++++++++mac:::::::::check mac succeed !!!!");
	return ERR_OK;
}

static void decodeMerchantDesc(char* buffer) {
    #if 0
	int i;
	int count = 0;

	LOG_DEBUG("size of buffer: %d", sizeof(buffer));

	LtvStructInfo ltvStructInfo[15];
	unsigned char tmpBuffer[512] = {0};
	unsigned char merchantFa[512] = {0};

	memset(ltvStructInfo, 0x00, sizeof(LtvStructInfo) * 15);
	
	LOG_DEBUG("before unpackLtv");

	count = unpackLtv(buffer, ltvStructInfo);

	LOG_DEBUG("Count is [%d]", count);

	for (i = 0; i < count; i++)
	{
		int tag = sdkatoi(ltvStructInfo[i].tag);
		LOG_DEBUG("TAG [%d]", tag);
		switch (tag)
		{
		case 31:
		{
			int index = 0;
			memset(tmpBuffer, 0x00, sizeof(tmpBuffer));
			memset(config.MerchantName, 0x00, sizeof(config.MerchantName));
			pubHexStringToBytes(ltvStructInfo[i].data, ltvStructInfo[i].len - 1, tmpBuffer);
			do
			{
				//						LOG_DEBUG("decodeMerchantDesc >> tmpBuffer[%d] >> %02X", index, tmpBuffer[index]);
				if ((tmpBuffer[index] == 0x5C) || (tmpBuffer[index] == 0x00))
				{
					LOG_DEBUG("decodeMerchantDesc >> %02X >> BREAK", tmpBuffer[index]);
					break;
				}
				merchantFa[index] = tmpBuffer[index];
				index++;

			} while (1);
			merchantFa[index] = 0x00;
			LOG_DEBUG("[*** merchantFa (%d) : %s ***]", index, merchantFa);
			convertStrNoNumber((char *)merchantFa, index, 0, 4, 5, config.MerchantName); // index, 0, 4, 5
			LOG_DEBUG("[*** config.MerchantName (%d) : %s ***]", strlen(config.MerchantName), config.MerchantName);
			break;
		}
		case 32:
		{
			//					memset (config.MerchantAddress, 0x00, sizeof(config.MerchantAddress));
			//					convertStr (ltvStructInfo[i].data, strlen(ltvStructInfo[i].data), 0, config.MerchantAddress);
			break;
		}
		case 34:
		{
			memset(config.MerchantPhone, 0x00, sizeof(config.MerchantPhone));
			pubHexStringToBytes(ltvStructInfo[i].data, ltvStructInfo[i].len - 1, (unsigned char *)config.MerchantPhone);
			// strcpy (config.MerchantPhone, ltvStructInfo[i].data);
			LOG_DEBUG(" &&&&&&&&&&&&&&&&&& ltvStructInfo[i].data [%s] len[%d] config.MerchantPhone [%s] &&&&&&&&&&&&&&&&&&&&&&& ",
					 ltvStructInfo[i].data, ltvStructInfo[i].len, config.MerchantPhone);
			break;
		}
		case 35:
		{
			//					memset (config.MerchantPostalCode, 0x00, sizeof(config.MerchantPostalCode));
			//					hexStringToBytes(ltvStructInfo[i].data, (unsigned char *)config.MerchantPostalCode);
			break;
		}
		case 50:
		{
			int ret = 0;
			int counter = 2;
			char part[2 + 1] = {0};
			int dateTime[6] = {0};
			unsigned char tempBuf[512] = {0};
			char switchDateTime[16] = {0};
			CLRBUF(tempBuf);
			CLRBUF(switchDateTime);
			LOG_DEBUG("dateTime(%d) [%s]", ltvStructInfo[i].len, ltvStructInfo[i].data);
			pubHexStringToBytes(ltvStructInfo[i].data, ltvStructInfo[i].len - 1, tempBuf);
			memset(part, 0x00, sizeof(part));
			memcpy(part, tempBuf + counter, 2);
			dateTime[0] = my_atoi(part);
			counter += 2;
			memset(part, 0x00, sizeof(part));
			memcpy(part, tempBuf + counter, 2);
			dateTime[1] = my_atoi(part);
			counter += 2;
			memset(part, 0x00, sizeof(part));
			memcpy(part, tempBuf + counter, 2);
			dateTime[2] = my_atoi(part);
			counter += 2;
			memset(part, 0x00, sizeof(part));
			memcpy(part, tempBuf + counter, 2);
			dateTime[3] = my_atoi(part);
			counter += 2;
			memset(part, 0x00, sizeof(part));
			memcpy(part, tempBuf + counter, 2);
			dateTime[4] = my_atoi(part);
			counter += 2;
			memset(part, 0x00, sizeof(part));
			memcpy(part, tempBuf + counter, 2);
			dateTime[5] = my_atoi(part);
			counter += 2;
			ret = dtSetSystemDateTime(dateTime);
			LOG_DEBUG("setSwitchDateTime::[%d] = dtSetSystemDateTime(%s);", ret, tempBuf); // sysSetDateTime(48320218); 20231212231334
			break;
		}
		case 41:
		{
			char temp[128] = {0};
			int pinLen = 0;
			unsigned char pinTemp[64];
			unsigned char pinBytes[32];
			unsigned char pinData[32];
			unsigned int outLen = 0;

			strcpy(temp, ltvStructInfo[i].data);
			pinLen = strlen(temp) / 2;
			LOG_DEBUG("parseVoucherTransactionSIPA::temp[%s]pinLen[%d]", temp, pinLen);

			memset(pinTemp, 0x00, sizeof(pinTemp));
			memset(pinBytes, 0x00, sizeof(pinBytes));
			memset(pinData, 0x00, sizeof(pinData));

			pubHexStringToBytes(temp, pinLen, pinTemp); // Pin
			//TraceExt(pinTemp, pinLen, "[Voucher PIN]");
			pinLen /= 2;
			pubHex2data(pinBytes, pinTemp, pinLen);
			LOG_DEBUG("parseVoucherTransactionSIPA::pinLen[%d]", pinLen);
			//TraceExt(pinBytes, pinLen, "[Voucher PIN Ex]");

			pedDecrypt(TDK_INDEX, pinBytes, 16, pinData);
			strcpy(currentTransaction.sqlTrx.PaymentId, (const char *)pinData);
			//TraceExt(pinTemp, 16, "VoucherPin [%s]", currentTransaction.sqlTrx.PaymentId);
			break;
		}
		case 80:
		{
			memset(currentTransaction.sqlTrx.BillId, 0x00, sizeof(currentTransaction.sqlTrx.BillId));
			LOG_DEBUG("Tag 80 [%s]", ltvStructInfo[i].data);
			pubHexStringToBytes(ltvStructInfo[i].data, ltvStructInfo[i].len - 1, currentTransaction.sqlTrx.BillId);
			LOG_DEBUG("*****************************************");
			LOG_DEBUG("FaraBILLID [%s]", currentTransaction.sqlTrx.BillId);
			LOG_DEBUG("*****************************************");
			break;
		}

		case 40:
		{
			pubHexStringToBytes(ltvStructInfo[i].data, ltvStructInfo[i].len - 1, (unsigned char *)currentTransaction.sqlTrx.BillId);
			LOG_DEBUG("VoucherSerial [%s]", currentTransaction.sqlTrx.BillId);
			break;
		}
		case 94:
		{
			setting.ForceTMS = 1;
			pubSaveSetting();
			break;
		}
		case 98:
		{
			memset(config.MerchantUniqueId, 0x00, sizeof(config.MerchantUniqueId));
			pubHexStringToBytes(ltvStructInfo[i].data, ltvStructInfo[i].len - 1, (unsigned char *)config.MerchantUniqueId);
			LOG_DEBUG("*****************************************");
			LOG_DEBUG("*****************************************");
			LOG_DEBUG("config.MerchantUniqueId [%s]", config.MerchantUniqueId);
			LOG_DEBUG("*****************************************");
			LOG_DEBUG("*****************************************");
			pubSaveConfig();
			break;
		}
		case 99:
		{
			LOG_DEBUG("TAG 99");
		}
		}

	} // for (i=0; i<count; i++)
#endif
}

Error_t isoBuildLogOn(ByteArray *buf) {
    iso8583()->reset();
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    LOG_DEBUG("dn = %s", sn );
    DEFINE_STRING(privateData, 128);
	setCommonLtv(sn, PNA_APP_VERSION, 0 /*language*/, privateData);
    DateTime *dt = OOP_CALL(sys(), getDateTime);
    /* set ISO message fields */
    iso8583()->setMTI((const DL_UINT8 *)MTI_VAL_LOG_ON);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8 *)PRC_LOG_ON);
    iso8583()->setStr(ELEMENT_STAN, (const DL_UINT8 *)"000001");
    iso8583()->setStr(ELEMENT_TIME_LOCAL_TRANSACTION, (const DL_UINT8 *)dt->time);
    iso8583()->setStr(ELEMENT_DATE_LOCAL_TRANSACTION, (const DL_UINT8 *)dt->date + 2);
    DEFINE_STRING(nni, 8);
    prependZerosInt(settings()->server.mainServerNii, 4, nni, sizeof(nni));
    iso8583()->setStr(ELEMENT_NETWORK_INTL_ID, (const DL_UINT8 *)nni);
#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(32, (const DL_UINT8 *)setting.AcquireIIN, &isoMsg); // IIN
#endif
    iso8583()->setStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, (const DL_UINT8 *)privateData);
    iso8583()->setStr(ELEMENT_SECURITY_CONTROL_INFO, (const DL_UINT8 *)SecRelControlInfo);
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

static Error_t isoParseLogOnResponse(ByteArray *buf)
{
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
    DEFINE_STRING(f48, 16);
    iso8583()->getStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, f48);
    DEFINE_STRING(f41, 16);
    iso8583()->getStr(ELEMENT_TERMINAL_ID, f41);
    LOG_DEBUG("terminal number = %s", f41);
	decodeMerchantDesc(f48);
    settings()->save();
	return ERR_OK;
}

Error_t isoBuildCfg(ByteArray *buf) {
    iso8583()->reset();
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
    LOG_DEBUG("dn = %s", sn );
    DEFINE_STRING(privateData, 128);
	setCommonLtv(sn, PNA_APP_VERSION, 0 /*language*/, privateData);
    DateTime *dt = OOP_CALL(sys(), getDateTime);
    /* set ISO message fields */
    iso8583()->setMTI((const DL_UINT8 *)MTI_VAL_CFG);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8 *)PRC_CFG);
    iso8583()->setStr(ELEMENT_STAN, (const DL_UINT8 *)"000001");
    iso8583()->setStr(ELEMENT_TIME_LOCAL_TRANSACTION, (const DL_UINT8 *)dt->time);
    iso8583()->setStr(ELEMENT_DATE_LOCAL_TRANSACTION, (const DL_UINT8 *)dt->date + 2);
    DEFINE_STRING(nni, 8);
    prependZerosInt(settings()->server.mainServerNii, 4, nni, sizeof(nni));
    iso8583()->setStr(ELEMENT_NETWORK_INTL_ID, (const DL_UINT8 *)nni);
#if defined REMOTE_KEY_INJECTION
    (void)SIPA_ISO8583_MSG_SetField_Str(32, (const DL_UINT8 *)setting.AcquireIIN, &isoMsg); // IIN
#endif
    iso8583()->setStr(ELEMENT_TERMINAL_ID, (const DL_UINT8 *)privateData);
    iso8583()->setStr(ELEMENT_ADDITIONAL_DATA_PRIVATE, (const DL_UINT8 *)settings()->terminal.terminalNo);
    iso8583()->setStr(ELEMENT_SECURITY_CONTROL_INFO, (const DL_UINT8 *)SecRelControlInfo);
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

static Error_t isoParseCfgResponse(ByteArray *buf)
{
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

static const IsoTransaction templates[] = {
    {
        .requestMti  = MTI_LOG_ON,
        .responseMti = MTI_LOG_ON_RESPONSE,
        .builder     = isoBuildLogOn,
        .parser      = isoParseLogOnResponse
    },
    {
        .requestMti  = MTI_CFG,
        .responseMti = MTI_CFG_RESPONSE,
        .builder     = isoBuildCfg,
        .parser      = isoParseCfgResponse
    }
};

Error_t isoBuild(MTI_t mti, ByteArray *buf) {
    for (size_t i = 0; i < sizeof(templates) / sizeof(templates[0]); i++) {
        if (templates[i].requestMti == mti) {
            return templates[i].builder(buf);
        }
    }
    return ERR_NOK;
}

Error_t isoParse(MTI_t mti, ByteArray *buf) {
    RETURN_VALUE_IF_NULL(buf, ;, ERR_NULL_PARAMETER);
    IsoStatus_t st = iso8583()->parse(buf->data, buf->len);
    LOG_DEBUG("iso parse error = %d", st);
    RETURN_VALUE_IF_NOT(st, ISO_OK, ;, ERR_NOK);
    // Check responce code
    DEFINE_STRING(f39, 8);
    iso8583()->getStr(ELEMENT_RESPONSE_CODE, f39);
    int respCode = libAtoi(f39);
    LOG_DEBUG("Txn responce code = %d", respCode);
    RETURN_VALUE_IF_NOT(respCode , 0, ;, ERR_NOK);

    for (size_t i = 0; i < sizeof(templates) / sizeof(templates[0]); i++) {
        if (templates[i].responseMti == mti) {
            return templates[i].parser(buf);
        }
    }

    return ERR_NOK;
}