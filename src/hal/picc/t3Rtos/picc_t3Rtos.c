#ifdef DEVICE_TRENDITT3RTOS

#include "picc_t3Rtos.h"

#include "sdkPicc.h"
#include "sdkIcc.h"

#include "logger.h"

/* -----------------------------------------------------------
 * Error translation
 * ----------------------------------------------------------- */

static PiccErr_t translateSdkErr(int32_t err) {
    switch (err) {
    case SDK_PICC_OK:
        return PICC_ERR_OK;

    case SDK_PICC_ERR_INPUT:
        return PICC_ERR_BAD_PARAMETER;

    case SDK_PICC_ERR_TIMEOUT:
        return PICC_ERR_TIMEOUT;

    default:
        return PICC_ERR_OTHER;
    }
}

/* -----------------------------------------------------------
 * Type translation
 * ----------------------------------------------------------- */

static SDK_PICC_TYPE translateTechnology(PiccTechMask_t technologies) {
    uint32_t sdkType = 0;

    if (technologies & PICC_TECH_A)
        sdkType |= PICC_TYPE_A;

    if (technologies & PICC_TECH_B)
        sdkType |= PICC_TYPE_B;

    if (technologies & PICC_TECH_FELICA)
        sdkType |= PICC_TYPE_FELICA;

    return (SDK_PICC_TYPE)sdkType;
}

static PiccRemoveMode_t normalizeRemoveMode(PiccRemoveMode_t mode) {
    switch (mode) {
    case PICC_REMOVE_HALT:
    case PICC_REMOVE_WAIT:
    case PICC_REMOVE_EMV:
        return mode;

    default:
        return PICC_REMOVE_HALT;
    }
}

static SDK_PICC_REMOVE_MODE translateRemoveMode(PiccRemoveMode_t mode) {
    mode = normalizeRemoveMode(mode);

    switch (mode) {
    case PICC_REMOVE_WAIT:
        return PICC_REMOVE_MODE_R;

    case PICC_REMOVE_EMV:
        return PICC_REMOVE_MODE_E;

    case PICC_REMOVE_HALT:
    default:
        return PICC_REMOVE_MODE_H;
    }
}

/* -----------------------------------------------------------
 * Init
 * ----------------------------------------------------------- */

static PiccErr_t init(Picc* self) {
    if (self == NULL)
        return PICC_ERR_BAD_PARAMETER;

    if (self->opened)
        return PICC_ERR_OK;

    int32_t ret = sdkPiccOpen();

    self->error = translateSdkErr(ret);

    if (self->error != PICC_ERR_OK)
        return self->error;

    self->opened        = true;
    self->powered       = false;
    self->eventReported = false;

    self->data.cardType = PICC_CARD_NONE;

    return PICC_ERR_OK;
}

/* -----------------------------------------------------------
 * Close
 * ----------------------------------------------------------- */

static PiccErr_t close(Picc* self) {
    if (self == NULL)
        return PICC_ERR_BAD_PARAMETER;

    if (!self->opened)
        return PICC_ERR_OK;

    int32_t ret = sdkPiccClose();

    self->error = translateSdkErr(ret);

    if (self->error != PICC_ERR_OK)
        return self->error;

    self->opened        = false;
    self->powered       = false;
    self->eventReported = false;

    self->data.cardType = PICC_CARD_NONE;

    return PICC_ERR_OK;
}

/* -----------------------------------------------------------
 * Power on / start RF search
 * ----------------------------------------------------------- */

static PiccErr_t powerOn(Picc* self, PiccTechMask_t technologies) {
    if (self == NULL)
        return PICC_ERR_BAD_PARAMETER;

    if (!self->opened) {
        self->error = PICC_ERR_NOT_INITIALIZED;
        return self->error;
    }

    if (technologies == 0) {
        self->error = PICC_ERR_BAD_PARAMETER;
        return self->error;
    }

    SDK_PICC_TYPE sdkType = translateTechnology(technologies);

    int32_t ret = sdkPiccPowerOn(sdkType);

    self->error = translateSdkErr(ret);

    if (self->error != PICC_ERR_OK)
        return self->error;

    self->powered = true;

    return PICC_ERR_OK;
}

/* -----------------------------------------------------------
 * Detect
 * ----------------------------------------------------------- */

static PiccCardType_t detect(Picc* self) {
    if (self == NULL)
        return PICC_CARD_UNKNOWN;

    if (!self->opened || !self->powered) {
        self->error = PICC_ERR_NOT_INITIALIZED;
        return PICC_CARD_UNKNOWN;
    }

    int32_t ret = sdkPiccDetect();

    /*
     * sdkPiccDetect doesn't return SDK_PICC_OK for success.
     *
     * Its successful result IS the detected card type.
     */
    switch (ret) {
    case PICC_NO_CARD:
        self->data.cardType = PICC_CARD_NONE;
        self->error         = PICC_ERR_OK;
        break;

    case PICC_CARD_TYPE_A:
        self->data.cardType = PICC_CARD_A;
        self->error         = PICC_ERR_OK;
        break;

    case PICC_CARD_TYPE_B:
        self->data.cardType = PICC_CARD_B;
        self->error         = PICC_ERR_OK;
        break;

    case PICC_CARD_TYPE_A_MEMORY:
        self->data.cardType = PICC_CARD_A_MEMORY;
        self->error         = PICC_ERR_OK;
        break;

    case PICC_CARD_TYPE_B_MEMORY:
        self->data.cardType = PICC_CARD_B_MEMORY;
        self->error         = PICC_ERR_OK;
        break;

    case PICC_CARD_TYPE_FELICA:
        self->data.cardType = PICC_CARD_FELICA;
        self->error         = PICC_ERR_OK;
        break;

    case PICC_CARD_TYPE_MORE:
        self->data.cardType = PICC_CARD_MULTIPLE;
        self->error         = PICC_ERR_OK;
        break;

    default:
        self->data.cardType = PICC_CARD_UNKNOWN;
        self->error         = translateSdkErr(ret);
        break;
    }

    return self->data.cardType;
}

/* -----------------------------------------------------------
 * Remove
 * ----------------------------------------------------------- */

static PiccErr_t removeCard(Picc* self, PiccRemoveMode_t mode) {
    if (self == NULL)
        return PICC_ERR_BAD_PARAMETER;

    if (!self->opened) {
        self->error = PICC_ERR_NOT_INITIALIZED;
        return self->error;
    }

    int32_t ret = sdkPiccRemove(translateRemoveMode(mode));

    self->error = translateSdkErr(ret);

    if (self->error != PICC_ERR_OK)
        return self->error;

    self->data.cardType = PICC_CARD_NONE;
    self->eventReported = false;

    return PICC_ERR_OK;
}

/* -----------------------------------------------------------
 * APDU
 * ----------------------------------------------------------- */

static PiccErr_t exchangeApdu(Picc* self, const ApduReq_t* request,
                              ApduResp_t* response) {
    if (self == NULL || request == NULL || response == NULL) {
        return PICC_ERR_BAD_PARAMETER;
    }

    if (!self->opened || !self->powered) {
        self->error = PICC_ERR_NOT_INITIALIZED;
        return self->error;
    }

    /*
     * We need the exact ApduSend / ApduResp structure definitions
     * from sdkIcc.h before implementing this conversion safely.
     *
     * Conceptually:
     *
     *     ApduSend sdkRequest;
     *     ApduResp sdkResponse;
     *
     *     convert request -> sdkRequest
     *
     *     int ret =
     *         sdkPiccExchangeApdu(
     *             &sdkRequest,
     *             &sdkResponse);
     *
     *     convert sdkResponse -> response
     *
     * I do NOT recommend exposing ApduSend/ApduResp in picc.h.
     */

    (void)request;
    (void)response;

    self->error = PICC_ERR_NOT_SUPPORTED;

    return self->error;
}

/* -----------------------------------------------------------
 * Constructor
 * ----------------------------------------------------------- */

OOP_CTOR(PiccT3Rtos) {
    self->base.vtable.init         = init;
    self->base.vtable.close        = close;
    self->base.vtable.powerOn      = powerOn;
    self->base.vtable.detect       = detect;
    self->base.vtable.exchangeApdu = exchangeApdu;
    self->base.vtable.remove       = removeCard;
}

#endif