#include "isoBuilder.h"
#include "sys/sys.h"
#include "settings/settings.h"
#include "ped/ped.h"

#define MTI_VAL_LOG_ON              "0800"

Error_t isoBuildLogOn(ByteArray *buf) {
    iso8583()->reset();
    DEFINE_STRING(privateData, 128);
    DEFINE_STRING(sn, 32);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));
	setCommonLtv(sn, PNA_APP_VERSION, 0 /*language*/, privateData);
    DateTime *dt = OOP_CALL(sys(), getDateTime);
    /* set ISO message fields */
    iso8583()->setMTI((const DL_UINT8 *)MTI_VAL_LOG_ON);
    iso8583()->setStr(ELEMENT_PROCESSING_CODE, (const DL_UINT8 *)PRC_LOG_ON);
    iso8583()->setStr(ELEMENT_STAN, (const DL_UINT8 *)"0001");
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

static const IsoTemplate templates[] = {
    { MTI_LOG_ON, isoBuildLogOn },
};

Error_t isoBuild(MTI_t mti, ByteArray *buf)
{
    for (size_t i = 0; i < sizeof(templates) / sizeof(templates[0]); i++) {
        if (templates[i].mti == mti) {
            return templates[i].builder(buf);
        }
    }

    return ERR_NOK;
}