#ifdef DEVICE_TRENDITT3RTOS

#include "picc_t3Rtos.h"
#include "posplatform.h"
#include "sdkPicc.h"
#include "../../dev/dev.h"

static ApduSend apduSend;
static ApduResp apduResp;

static PiccErr_t translateSdkErr(int err) {
    PiccErr_t piccErr;
    return piccErr;
}

static void init(Picc* picc) {

}

static PiccCardType_t detect(Picc* picc) {
    PiccCardType_t cardType = PICC_CARD_NO_CARD;
    int32_t ret = sdkPiccDetect();
    switch (ret) {
    case PICC_CARD_TYPE_MORE:
        cardType = PICC_CARD_MORE;
        break;
    case PICC_CARD_TYPE_A:
        cardType = PICC_CARD_A;
        break;
    case PICC_CARD_TYPE_B:
        cardType = PICC_CARD_B;
        break;
    case PICC_CARD_TYPE_A_MEMORY:
        cardType = PICC_CARD_A_MEMORY;
        break;
    case PICC_CARD_TYPE_B_MEMORY:
        cardType = PICC_CARD_B_MEMORY;
        break;
    case PICC_CARD_TYPE_FELICA:
        cardType = PICC_CARD_FELICA;
        break;
    default:
        break;
    }
    return cardType;
}

static PiccErr_t exchangeApdu(Picc* picc) {
    PiccErr_t err = translateSdkErr(sdkPiccExchangeApdu(&apduSend, &apduResp));
    return err;
}

static ApduSend_t *getSendAdpu(Picc* picc) {
    picc->sendAdpu.dataIn = apduSend.mDataIn;
    picc->sendAdpu.command = apduSend.mCommand;
    picc->sendAdpu.lc = &apduSend.mLc;
    picc->sendAdpu.le = &apduSend.mLe;
}

static ApduResp_t *getRespAdpu(Picc* picc) {
    picc->respAdpu.dataOut = apduResp.mDataOut;
    picc->respAdpu.lenOut = &apduResp.mLenOut;
    picc->respAdpu.swa = &apduResp.mSWA;
    picc->respAdpu.swb = &apduResp.mSWB;
}

OOP_CTOR(PiccT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.detect = detect;
    self->base.vtable.exchangeApdu = exchangeApdu;
    self->base.vtable.getSendAdpu = getSendAdpu;
    self->base.vtable.getRespAdpu = getRespAdpu;
}

#endif
