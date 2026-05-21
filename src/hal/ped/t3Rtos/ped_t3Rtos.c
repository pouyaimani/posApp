#ifdef DEVICE_TRENDITT3RTOS

#include "ped_t3Rtos.h"
#include "sdkPed.h"
#include "utility/arith.h"

#define MASTER_KEY_INDEX 0
#define MAC_KEY_INDEX 1
#define PIN_KEY_INDEX 2
#define DTK_KEY_INDEX 3

static PedErr_t translateSdkErr(int err) {
    PedErr_t pedErr;
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

static PedErr_t injectKey(Ped* self, PedKeyType_t type, uint8_t key, size_t len) {
    int result = 0;
    PedKeyInfo keyInfo = {0};
    PedKeyCheckValue checkValue = {0};
    switch (type) {
    case PED_MASTER_KEY:
        keyInfo.mSrcKeyType = 0; // Plain
        keyInfo.mDestKeyType = PED_KEY_TDES_TMK;
        keyInfo.mSrcKeyIndex = 0;
        keyInfo.mDestKeyIndex = MASTER_KEY_INDEX;
    case PED_PIN_KEY:
        keyInfo.mSrcKeyType = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType = PED_KEY_TDES_TPK;
        keyInfo.mSrcKeyIndex = MASTER_KEY_INDEX;
        keyInfo.mDestKeyIndex = PIN_KEY_INDEX;
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
    case PED_DATA_KEY:
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType = PED_KEY_TDK;
        keyInfo.mSrcKeyIndex = MASTER_KEY_INDEX;
        keyInfo.mDestKeyIndex = DTK_KEY_INDEX;
    case PED_MAC_KEY:
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType = PED_KEY_TAK;
        keyInfo.mSrcKeyIndex = MASTER_KEY_INDEX;
        keyInfo.mDestKeyIndex = DTK_KEY_INDEX;
    default:
        return PED_ERR_INPUT;
    }
    keyInfo.mDestKeyLen = len;
    memcpy(keyInfo.mDestKey, key, len);

    result = sdkPedWriteKey(&keyInfo, &checkValue);
    return translateSdkErr(result);
}

static PedErr_t enterPinEntryMode(Ped* self) {
    int32_t ret = 0;
    u32 keyGroup = 0;
    SDK_PED_KEY_TYPE pedKeyType = PED_KEY_TDES_TPK;
    SDK_PED_PIN_MODE pedPinMode = PED_PIN_ISO_9564_0;
    int x0 = 31;
    int y0 = 133;
    int width = 262;
    int height = 41;

    ret = sdkPedEnterPinInputMode(keyGroup, pedKeyType, PIN_KEY_INDEX,
                                    pedPinMode, 0, PIN_MIN_LEN, PIN_MAX_LEN,
                                        PED_PIN_ENTRY_TIME_OUT);
    return translateSdkErr(ret);
}

static PedErr_t exitPinEntryMode(Ped* self) {
    return translateSdkErr(sdkPedExitPinInputMode());
}

static PedErr_t getPinBlock(char *pan, char* out) {
    u8 dataIn[300] = {0};
    PedPinBlockData pinBlockData;
    ascToBcd(dataIn + 2, (pan + strlen(pan) - 13), 12);
    int ret = sdkPedGetPinBlock(0, PED_KEY_TDES_TPK, 0, PED_PIN_ISO_9564_0, 0, dataIn, 8, &pinBlockData);
    if (out != NULL) {
        out[0] = pinBlockData.mPinBlockDataLen;
        memcpy(out + 1, pinBlockData.mPinbBockData, pinBlockData.mPinBlockDataLen);
    }
    return PED_ERR_OK;
}

OOP_CTOR(PedT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.injectKey = injectKey;
    self->base.vtable.enterPinEntryMode = enterPinEntryMode;
    self->base.vtable.exitPinEntryMode = exitPinEntryMode;
    self->base.vtable.getPinBlock = getPinBlock;
}

#endif