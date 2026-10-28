
#ifdef DEVICE_TRENDITT3RTOS

#include "ped_t3Rtos.h"
#include "sdkPed.h"
#include "utility/arith.h"
#include "common.h"
#include "sdkLog.h"
#include <sdkKey.h>
#include <string.h>

#define KEY_INDEX_MASTER 0

#define TMK_INDEX  0
#define TK_INDEX   1
#define LTAK_INDEX 2
#define TAK_INDEX  3
#define TPK_INDEX  4
#define TDK_INDEX  5
#define TMP_INDEX  6
#define TMH_INDEX  7

/**
 * Convert application PED key type to SDK key index.
 */
static int convertKeyIndex(PedKeyType_t key) {
    int keyIndex = -1;

    LOG_TRACE("PED: convertKeyIndex: start type=%d", (int)key);

    switch (key) {
    case PED_KEY_MASTER:
        keyIndex = TMK_INDEX;
        break;
    case PED_KEY_PIN:
        keyIndex = TPK_INDEX;
        break;
    case PED_KEY_DATA:
        keyIndex = TDK_INDEX;
        break;
    case PED_KEY_MAC:
        keyIndex = TAK_INDEX;
        break;
    default:
        LOG_TRACE("PED: convertKeyIndex: unsupported type=%d", (int)key);
        break;
    }

    LOG_TRACE("PED: convertKeyIndex: result=%d", keyIndex);
    return keyIndex;
}

/**
 * Translate SDK PED error to application PED error.
 */
static PedErr_t translateSdkErr(int err) {
    PedErr_t pedErr = PED_ERR_INPUT;

    switch (err) {
    case SDK_PED_OK:
        pedErr = PED_ERR_OK;
        break;
    default:
        LOG_TRACE("PED: translateSdkErr: unhandled SDK error=%d", err);
        break;
    }

    LOG_TRACE("PED: translateSdkErr: sdk=%d ped=%d",
              err, (int)pedErr);

    return pedErr;
}

/**
 * Initialize PED adapter.
 */
static void init(Ped* self) {
    (void)self;
    LOG_TRACE("PED: init: completed");
}

/**
 * Decrypt account data using TDK.
 *
 * Note: caller must provide sufficient output capacity.
 */
static PedErr_t encryptAccountData(Ped* self,
                                   void* buffer,
                                   int bufLen,
                                   void* decryptedData) {
    LOG_TRACE("PED: encryptAccountData: start inputLen=%d", bufLen);

    if (!self || !buffer || !decryptedData || bufLen <= 0) {
        LOG_TRACE("PED: encryptAccountData: invalid arguments");
        return PED_ERR_INPUT;
    }

    u32 keyGroup = 0;
    u8 keyIndex = TDK_INDEX;
    PedAccountData accData = {0};

    LOG_TRACE("PED: encryptAccountData: calling SDK group=%u index=%u",
              (unsigned int)keyGroup,
              (unsigned int)keyIndex);

    int err = sdkPedEncryptAccountData(
        keyGroup,
        PED_KEY_TDK,
        keyIndex,
        PED_ACCOUNTALG_TDES_DEC_ECB,
        0, 0,
        buffer,
        bufLen,
        &accData
    );

    LOG_TRACE("PED: encryptAccountData: SDK result=%d", err);

    if (err != SDK_PED_OK) {
        LOG_TRACE("PED: encryptAccountData: failed");
        return translateSdkErr(err);
    }

    if (accData.mAccountDataLen > sizeof(accData.mAccountBlockData)) {
        LOG_TRACE("PED: encryptAccountData: invalid SDK output length=%u",
                  (unsigned int)accData.mAccountDataLen);
        return PED_ERR_INPUT;
    }

    memcpy(decryptedData,
           accData.mAccountBlockData,
           accData.mAccountDataLen);

    LOG_TRACE("PED: encryptAccountData: success outputLen=%u",
              (unsigned int)accData.mAccountDataLen);

    return PED_ERR_OK;
}

/**
 * Inject a key into secure PED storage.
 *
 * Security:
 *   Never log raw key material.
 */
static PedErr_t injectKey(Ped* self,
                          PedKeyType_t type,
                          uint8_t* key,
                          size_t len) {
    LOG_TRACE("PED: injectKey: start type=%d length=%u",
              (int)type, (unsigned int)len);

    if (!self || !key || len == 0u) {
        LOG_TRACE("PED: injectKey: invalid arguments");
        return PED_ERR_INPUT;
    }

    int result = 0;
    PedKeyInfo keyInfo = {0};
    PedKeyCheckValue checkValue = {0};

    if (len > sizeof(keyInfo.mDestKey)) {
        LOG_TRACE("PED: injectKey: key exceeds destination capacity");
        return PED_ERR_INPUT;
    }

    switch (type) {
    case PED_KEY_MASTER:
        keyInfo.mSrcKeyType   = 0;
        keyInfo.mDestKeyType  = PED_KEY_TDES_TMK;
        keyInfo.mSrcKeyIndex  = 0;
        keyInfo.mDestKeyIndex = KEY_INDEX_MASTER;
        break;

    case PED_KEY_PIN:
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType   = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType  = PED_KEY_TDES_TPK;
        keyInfo.mSrcKeyIndex  = KEY_INDEX_MASTER;
        keyInfo.mDestKeyIndex = TPK_INDEX;
        break;

    case PED_KEY_DATA:
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType   = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType  = PED_KEY_TDK;
        keyInfo.mSrcKeyIndex  = KEY_INDEX_MASTER;
        keyInfo.mDestKeyIndex = TDK_INDEX;
        break;

    case PED_KEY_MAC:
        checkValue.mCheckMode = PED_CHECK_MODE_DES;
        keyInfo.mSrcKeyType   = PED_KEY_TDES_TMK;
        keyInfo.mDestKeyType  = PED_KEY_TAK;
        keyInfo.mSrcKeyIndex  = KEY_INDEX_MASTER;
        keyInfo.mDestKeyIndex = TAK_INDEX;
        break;

    default:
        LOG_TRACE("PED: injectKey: unsupported key type=%d",
                  (int)type);
        return PED_ERR_INPUT;
    }

    keyInfo.mDestKeyLen = len;
    memcpy(keyInfo.mDestKey, key, len);

    LOG_TRACE("PED: injectKey: srcType=%d srcIndex=%d "
              "destType=%d destIndex=%d",
              (int)keyInfo.mSrcKeyType,
              (int)keyInfo.mSrcKeyIndex,
              (int)keyInfo.mDestKeyType,
              (int)keyInfo.mDestKeyIndex);

    LOG_TRACE("PED: injectKey: calling sdkPedWriteKey");

    result = sdkPedWriteKey(&keyInfo, &checkValue);

    LOG_TRACE("PED: injectKey: SDK result=%d", result);

    if (result != SDK_PED_OK) {
        LOG_TRACE("PED: injectKey: failed type=%d", (int)type);
    } else {
        LOG_TRACE("PED: injectKey: success type=%d index=%d",
                  (int)type, (int)keyInfo.mDestKeyIndex);
    }

    return translateSdkErr(result);
}

/**
 * Retrieve Key Check Value (first 4 bytes).
 */
static PedErr_t getKcv(PedKeyType_t key, char* out, size_t size) {
    LOG_TRACE("PED: getKcv: start type=%d outputSize=%u",
              (int)key, (unsigned int)size);

    if (!out || size < 4u) {
        LOG_TRACE("PED: getKcv: invalid output buffer");
        return PED_ERR_INPUT;
    }

    int result = 0;
    u32 keyGroup = 0;
    unsigned char buffer[8] = {0};
    PedAccountData accData = {0};

    int keyIdx = convertKeyIndex(key);

    LOG_TRACE("PED: getKcv: keyIndex=%d", keyIdx);

    switch (keyIdx) {
    case TMK_INDEX:
        result = sdkPedEncryptAccountData(
            keyGroup, PED_KEY_TDES_TMK, TMK_INDEX,
            PED_ACCOUNTALG_TDES_ENC_ECB,
            0, 0, buffer, 8, &accData);
        break;

    case TPK_INDEX:
        result = sdkPedEncryptAccountData(
            keyGroup, PED_KEY_TDES_TPK, TPK_INDEX,
            PED_ACCOUNTALG_TDES_ENC_ECB,
            0, 0, buffer, 8, &accData);
        break;

    case TAK_INDEX:
        result = sdkPedEncryptAccountData(
            keyGroup, PED_KEY_TAK, TAK_INDEX,
            PED_ACCOUNTALG_TDES_ENC_ECB,
            0, 0, buffer, 8, &accData);
        break;

    case TDK_INDEX:
        result = sdkPedEncryptAccountData(
            keyGroup, PED_KEY_TDK, TDK_INDEX,
            PED_ACCOUNTALG_TDES_ENC_ECB,
            0, 0, buffer, 8, &accData);
        break;

    default:
        LOG_TRACE("PED: getKcv: invalid key index=%d", keyIdx);
        return PED_ERR_INPUT;
    }

    LOG_TRACE("PED: getKcv: SDK result=%d", result);

    if (result != SDK_PED_OK) {
        LOG_TRACE("PED: getKcv: failed keyIndex=%d", keyIdx);
        return translateSdkErr(result);
    }

    if (accData.mAccountDataLen < 4u) {
        LOG_TRACE("PED: getKcv: insufficient SDK output length=%u",
                  (unsigned int)accData.mAccountDataLen);
        return PED_ERR_INPUT;
    }

    memcpy(out, accData.mAccountBlockData, 4u);

    LOG_TRACE("PED: getKcv: success keyIndex=%d kcvLen=4",
              keyIdx);

    return PED_ERR_OK;
}

/**
 * Enter secure PIN entry mode.
 */
static PedErr_t enterPinEntryMode(Ped* self) {
    LOG_TRACE("PED: enterPinEntryMode: start");

    if (!self) {
        LOG_TRACE("PED: enterPinEntryMode: NULL self");
        return PED_ERR_INPUT;
    }

    u32 keyGroup = 0;
    SDK_PED_KEY_TYPE pedKeyType = PED_KEY_TDES_TPK;
    SDK_PED_PIN_MODE pedPinMode = PED_PIN_ISO_9564_0;

    LOG_TRACE("PED: enterPinEntryMode: keyIndex=%d "
              "minLen=%d maxLen=%d timeout=%d",
              TPK_INDEX,
              PIN_MIN_LEN,
              PIN_MAX_LEN,
              PED_PIN_ENTRY_TIME_OUT);

    int32_t ret = sdkPedEnterPinInputMode(
        keyGroup,
        pedKeyType,
        TPK_INDEX,
        pedPinMode,
        0,
        PIN_MIN_LEN,
        PIN_MAX_LEN,
        PED_PIN_ENTRY_TIME_OUT
    );

    LOG_TRACE("PED: enterPinEntryMode: SDK result=%d", (int)ret);

    return translateSdkErr(ret);
}

/**
 * Exit secure PIN entry mode.
 */
static PedErr_t exitPinEntryMode(Ped* self) {
    LOG_TRACE("PED: exitPinEntryMode: start");

    if (!self) {
        LOG_TRACE("PED: exitPinEntryMode: NULL self");
        return PED_ERR_INPUT;
    }

    int ret = sdkPedExitPinInputMode();

    LOG_TRACE("PED: exitPinEntryMode: SDK result=%d", ret);

    return translateSdkErr(ret);
}

/**
 * Retrieve encrypted PIN block.
 *
 * Security:
 *   Never log PAN, PIN, or encrypted PIN block.
 */
static PedErr_t getPinBlock(Ped* self,
                            char* pan,
                            char* out,
                            size_t len) {
    LOG_TRACE("PED: getPinBlock: start outputCapacity=%u",
              (unsigned int)len);

    if (!self || !pan || !out) {
        LOG_TRACE("PED: getPinBlock: invalid arguments");
        return PED_ERR_INPUT;
    }

    size_t panLen = strlen(pan);

    if (panLen < 13u) {
        LOG_TRACE("PED: getPinBlock: invalid PAN length");
        return PED_ERR_INPUT;
    }

    u8 dataIn[8] = {0};
    PedPinBlockData pinBlockData = {0};

    /* ISO 9564 format 0: 12 PAN digits excluding check digit. */
    const char* panDigits = pan + panLen - 13u;

    for (size_t i = 0u; i < 12u; ++i) {
        if (panDigits[i] < '0' || panDigits[i] > '9') {
            LOG_TRACE("PED: getPinBlock: invalid PAN digit");
            return PED_ERR_INPUT;
        }
    }

    ascToBcd(dataIn + 2, panDigits, 12);

    LOG_TRACE("PED: getPinBlock: calling sdkPedGetPinBlock "
              "keyIndex=%d mode=ISO9564_0",
              TPK_INDEX);

    int ret = sdkPedGetPinBlock(
        0,
        PED_KEY_TDES_TPK,
        TPK_INDEX,
        PED_PIN_ISO_9564_0,
        0,
        dataIn,
        8,
        &pinBlockData
    );

    LOG_TRACE("PED: getPinBlock: SDK result=%d", ret);

    if (ret != SDK_PED_OK) {
        LOG_TRACE("PED: getPinBlock: SDK failed");
        return translateSdkErr(ret);
    }

    if (pinBlockData.mPinBlockDataLen > len ||
        pinBlockData.mPinBlockDataLen >
            sizeof(pinBlockData.mPinbBockData)) {

        LOG_TRACE("PED: getPinBlock: insufficient/invalid buffer "
                  "required=%u capacity=%u",
                  (unsigned int)pinBlockData.mPinBlockDataLen,
                  (unsigned int)len);

        return PED_ERR_INPUT;
    }

    memcpy(out,
           pinBlockData.mPinbBockData,
           pinBlockData.mPinBlockDataLen);

    LOG_TRACE("PED: getPinBlock: success blockLen=%u",
              (unsigned int)pinBlockData.mPinBlockDataLen);

    return PED_ERR_OK;
}

/**
 * Generate MAC using TAK.
 *
 * Security:
 *   Never log raw MAC or input data.
 *
 * Note: caller must provide sufficient output capacity.
 */
static PedErr_t getMac(Ped* self,
                       size_t keyLen,
                       uint8_t* in,
                       size_t inLen,
                       uint8_t* out) {
    LOG_TRACE("PED: getMac: start keyLen=%u inputLen=%u",
              (unsigned int)keyLen,
              (unsigned int)inLen);

    if (!self || !in || !out) {
        LOG_TRACE("PED: getMac: invalid arguments");
        return PED_ERR_INPUT;
    }

    u32 keyGroup = 0;
    PedMacData macData = {0};

    SDK_PED_MAC_MODE mode =
        (keyLen == 8u) ? PED_MAC_X99 : PED_MAC_X919;

    LOG_TRACE("PED: getMac: mode=%s keyIndex=%d",
              (keyLen == 8u) ? "X9.9" : "X9.19",
              TAK_INDEX);

    int result = sdkPedGetMac(
        keyGroup,
        PED_KEY_TAK,
        TAK_INDEX,
        mode,
        0,
        0,
        in,
        inLen,
        &macData
    );

    LOG_TRACE("PED: getMac: SDK result=%d", result);

    if (result != SDK_PED_OK) {
        LOG_TRACE("PED: getMac: MAC generation failed");
        return translateSdkErr(result);
    }

    if (macData.mMacDataLen > sizeof(macData.mMacBlockData)) {
        LOG_TRACE("PED: getMac: invalid SDK MAC length=%u",
                  (unsigned int)macData.mMacDataLen);
        return PED_ERR_INPUT;
    }

    memcpy(out,
           macData.mMacBlockData,
           macData.mMacDataLen);

    LOG_TRACE("PED: getMac: success macLen=%u",
              (unsigned int)macData.mMacDataLen);

    return PED_ERR_OK;
}

/**
 * Poll PED keypad.
 *
 * Log only recognized control events to avoid flooding
 * the trace during polling or exposing PIN entry data.
 */
static PedKeyEv_t poll(Ped* self) {
    (void)self;

    PedKeyEv_t pkey;
    SDK_KEY_VALUE key = sdkKeyGet();

    switch (key) {
    case KEY_VALUE_CLEAR:
        pkey = PED_KEY_EV_CLEAR;
        LOG_TRACE("PED: poll: CLEAR event");
        break;

    case KEY_VALUE_UP:
        pkey = PED_KEY_EV_DIGIT;
        LOG_TRACE("PED: poll: DIGIT event");
        break;

    case KEY_VALUE_ENTER:
        pkey = PED_KEY_EV_ENTER;
        LOG_TRACE("PED: poll: ENTER event");
        break;

    case KEY_VALUE_ESC:
        pkey = PED_KEY_EV_CANCEL;
        LOG_TRACE("PED: poll: CANCEL event");
        break;

    default:
        pkey = PED_KEY_EV_NONE;
        break;
    }

    return pkey;
}

/**
 * Initialize Trendit T3 RTOS PED implementation.
 */
OOP_CTOR(PedT3Rtos) {
    LOG_TRACE("PED: PedT3Rtos constructor: start");

    self->base.vtable.init              = init;
    self->base.vtable.injectKey         = injectKey;
    self->base.vtable.enterPinEntryMode = enterPinEntryMode;
    self->base.vtable.exitPinEntryMode  = exitPinEntryMode;
    self->base.vtable.getPinBlock       = getPinBlock;
    self->base.vtable.getMac            = getMac;
    self->base.vtable.poll              = poll;
    self->base.vtable.getKcv            = getKcv;

    LOG_TRACE("PED: PedT3Rtos constructor: completed");
}

#endif /* DEVICE_TRENDITT3RTOS */
