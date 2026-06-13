#ifdef DEVICE_TRENDITT3RTOS

#include "ped_t3Rtos.h"
#include "sdkPed.h"
#include "utility/arith.h"
#include "common.h"

#define KEY_INDEX_MASTER            0

#define TMK_INDEX	0
#define TK_INDEX	1
#define LTAK_INDEX	2
#define TAK_INDEX	3
#define TPK_INDEX	4
#define TDK_INDEX	5
#define TMP_INDEX	6
#define TMH_INDEX	7

static PedErr_t translateSdkErr(int err) {
    LOG_DEBUG("Ped: sdk error = %d", err);
    PedErr_t pedErr = PED_ERR_INPUT;
    switch (err) {
    case SDK_PED_OK:
        pedErr = PED_ERR_OK;
        break;
    default:
        break;
    }
    return pedErr;
}

static void init(Ped* self) {
}
static PedErr_t encryptAccountData(Ped* self, void *buffer, 
                                    int bufLen, void *decryptedData) {
    u32 keyGroup = 0;
    u8 keyIndex = 5;
    PedAccountData accData = { 0 };
    int err = sdkPedEncryptAccountData(keyGroup, PED_KEY_TDK, keyIndex, 
        PED_ACCOUNTALG_TDES_DEC_ECB, 0, 0, buffer, bufLen, &accData);
    RETURN_VALUE_IF_NOT(err, SDK_PED_OK, ;, PED_ERR_INPUT);
    memcpy (decryptedData, accData.mAccountBlockData, accData.mAccountDataLen);
    return PED_ERR_OK;

}

static PedErr_t injectKey(Ped* self, PedKeyType_t type, uint8_t *key, size_t len) {
    RETURN_VALUE_IF_NULL(self, ;, PED_ERR_INPUT);
    int result = 0;
    PedKeyInfo keyInfo = {0};
    PedKeyCheckValue checkValue = {0};
    switch (type) {
    case PED_MASTER_KEY:
        LOG_DEBUG("Ped: ======================================");
        keyInfo.mSrcKeyType = 0; // Plain
        keyInfo.mDestKeyType = PED_KEY_TDES_TMK;
        keyInfo.mSrcKeyIndex = 0;
        keyInfo.mDestKeyIndex = KEY_INDEX_MASTER;
        break;
    case PED_PIN_KEY:
        LOG_DEBUG("Ped: ======================================");
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType = PED_KEY_TDES_TPK;
        keyInfo.mSrcKeyIndex = KEY_INDEX_MASTER;
        keyInfo.mDestKeyIndex = TPK_INDEX;
        break;
    case PED_DATA_KEY:
        LOG_DEBUG("Ped: ======================================");
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType = PED_KEY_TDK;
        keyInfo.mSrcKeyIndex = KEY_INDEX_MASTER;
        keyInfo.mDestKeyIndex = TDK_INDEX;
        break;
    case PED_MAC_KEY:
        LOG_DEBUG("Ped: ======================================");
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType = PED_KEY_TAK;
        keyInfo.mSrcKeyIndex = KEY_INDEX_MASTER;
        keyInfo.mDestKeyIndex = TAK_INDEX;
        break;
    default:
        return PED_ERR_INPUT;
    }
    keyInfo.mDestKeyLen = len;
    memcpy(keyInfo.mDestKey, key, len);
    LOG_DEBUG("Ped: Key len = %d, key = %s", len, key);

    result = sdkPedWriteKey(&keyInfo, &checkValue);
    return translateSdkErr(result);
}

static PedErr_t enterPinEntryMode(Ped* self) {
    RETURN_VALUE_IF_NULL(self, ;, PED_ERR_INPUT);
    int32_t ret = 0;
    u32 keyGroup = 0;
    SDK_PED_KEY_TYPE pedKeyType = PED_KEY_TDES_TPK;
    SDK_PED_PIN_MODE pedPinMode = PED_PIN_ISO_9564_0;
    int x0 = 31;
    int y0 = 133;
    int width = 262;
    int height = 41;

    ret = sdkPedEnterPinInputMode(keyGroup, pedKeyType, TPK_INDEX,
                                    pedPinMode, 0, PIN_MIN_LEN, PIN_MAX_LEN,
                                        PED_PIN_ENTRY_TIME_OUT);
    return translateSdkErr(ret);
}

static PedErr_t exitPinEntryMode(Ped* self) {
    RETURN_VALUE_IF_NULL(self, ;, PED_ERR_INPUT);
    return translateSdkErr(sdkPedExitPinInputMode());
}

static PedErr_t getPinBlock(Ped* self, char *pan, char* out) {
    RETURN_VALUE_IF_NULL(self, ;, PED_ERR_INPUT);
    RETURN_VALUE_IF_NULL(pan, ;, PED_ERR_INPUT);
    RETURN_VALUE_IF_NULL(out, ;, PED_ERR_INPUT);
    u8 dataIn[300] = {0};
    PedPinBlockData pinBlockData;
    ascToBcd(dataIn + 2, (pan + strlen(pan) - 13), 12);
    int ret = sdkPedGetPinBlock(0, PED_KEY_TDES_TPK, 0, 
                    PED_PIN_ISO_9564_0, 0, dataIn, 8, &pinBlockData);
    if (out != NULL) {
        out[0] = pinBlockData.mPinBlockDataLen;
        memcpy(out + 1, pinBlockData.mPinbBockData, 
                pinBlockData.mPinBlockDataLen);
    }
    return PED_ERR_OK;
}

static PedErr_t getMac(Ped* self, size_t keyLen, uint8_t *in, size_t inLen, uint8_t *out) {
    RETURN_VALUE_IF_NULL(self, ;, PED_ERR_INPUT);
    RETURN_VALUE_IF_NULL(in, ;, PED_ERR_INPUT);
    RETURN_VALUE_IF_NULL(out, ;, PED_ERR_INPUT);
	PedErr_t result = 0;
	u32 keyGroup = 0;
	PedMacData macData = {0};
    SDK_PED_MAC_MODE mode = (keyLen == 8) ? PED_MAC_X99 : PED_MAC_X919;

	result = sdkPedGetMac(keyGroup, PED_KEY_TAK, KEY_INDEX_MASTER, 
                            mode, 0, 0, in, inLen, &macData); // PED_MAC_X99, PED_MAC_X919
	memcpy(out, macData.mMacBlockData, macData.mMacDataLen);

	return result;
}

OOP_CTOR(PedT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.injectKey = injectKey;
    self->base.vtable.enterPinEntryMode = enterPinEntryMode;
    self->base.vtable.exitPinEntryMode = exitPinEntryMode;
    self->base.vtable.getPinBlock = getPinBlock;
    self->base.vtable.getMac = getMac;
}

#endif