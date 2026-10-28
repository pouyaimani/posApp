#ifdef DEVICE_TRENDITT3RTOS

#include "picc_t3Rtos.h"

#include "sdkPicc.h"

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
 * Activate
 * ----------------------------------------------------------- */
static PiccErr_t activate(Picc* self) {
    ddi_rf_activate();
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

        self->error =
            (ret == SDK_PICC_OK) ? PICC_ERR_OTHER : translateSdkErr(ret);

        if (self->error == PICC_ERR_OK)
            self->error = PICC_ERR_OTHER;

        PICC_LOG("PICC: unexpected detect result=%ld error=%d", (long)ret,
                 (int)self->error);
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

/** Erase temporary APDU buffers without allowing optimization to remove it. */
static void wipeApduBuffer(void* buffer, size_t size) {
    volatile uint8_t* p = (volatile uint8_t*)buffer;

    while (size != 0u) {
        *p++ = 0u;
        --size;
    }
}

/**
 * Exchange one APDU with an already-opened, powered contactless reader.
 *
 * Request:
 *   command: points to four bytes {CLA, INS, P1, P2}.
 *   lc:      points to the command-data length, at most 512.
 *   dataIn:  readable for *lc bytes; may be NULL when *lc == 0.
 *   le:      expected response-data length, at most 512.
 *            0 means no response data requested.
 *            Use 256 for an unknown short-APDU response length.
 *
 * Response:
 *   lenOut:  IN: writable capacity of dataOut.
 *            OUT: bytes copied, excluding SW1/SW2; zero on failure.
 *   dataOut: writable for the supplied capacity.
 *            May be NULL only when capacity is zero.
 *   swa/swb: receive the separate status bytes; zero on failure.
 *
 * Length/status pointers must reference separate valid uint32_t objects
 * and must not overlap payload buffers.
 *
 * Polling and other PICC operations must be suspended/serialized by the
 * caller throughout this synchronous exchange.
 *
 * PICC_ERR_OK means transport succeeded. The caller must interpret SW1/SW2;
 * a card status other than 0x9000 is not automatically a transport failure.
 */
static PiccErr_t exchangeApdu(Picc* self, const ApduReq_t* request,
                              ApduResp_t* response) {
    ApduSend sdkRequest  = {0};
    ApduResp sdkResponse = {0};

    uint32_t  lc;
    uint32_t  le;
    uint32_t  capacity;
    int32_t   sdkResult;
    PiccErr_t error = PICC_ERR_BAD_PARAMETER;

    if (self == NULL) {
        PICC_LOG("PICC APDU rejected: NULL reader");
        return PICC_ERR_BAD_PARAMETER;
    }

    if (request == NULL || response == NULL) {
        PICC_LOG("PICC APDU rejected: requestPresent=%d responsePresent=%d",
                 (int)(request != NULL), (int)(response != NULL));
        goto cleanup;
    }

    if (response->lenOut == NULL || response->swa == NULL ||
        response->swb == NULL) {
        PICC_LOG("PICC APDU rejected: missing response length/status pointer");
        goto cleanup;
    }

    /* Capture capacity before initializing output metadata. */
    capacity          = *response->lenOut;
    *response->lenOut = 0u;
    *response->swa    = 0u;
    *response->swb    = 0u;

    if (request->command == NULL || request->lc == NULL ||
        request->le == NULL) {
        PICC_LOG("PICC APDU rejected: missing command/length pointer");
        goto cleanup;
    }

    lc = *request->lc;
    le = *request->le;

    PICC_LOG("PICC APDU: begin lc=%lu le=%lu capacity=%lu opened=%d powered=%d",
             (unsigned long)lc, (unsigned long)le, (unsigned long)capacity,
             (int)self->opened, (int)self->powered);

    if (!self->opened || !self->powered) {
        PICC_LOG("PICC APDU rejected: reader is not opened and powered");
        error = PICC_ERR_NOT_INITIALIZED;
        goto cleanup;
    }

    if (lc > sizeof(sdkRequest.mDataIn)) {
        PICC_LOG("PICC APDU rejected: lc=%lu exceeds SDK capacity=%lu",
                 (unsigned long)lc, (unsigned long)sizeof(sdkRequest.mDataIn));
        goto cleanup;
    }

    /* This adapter supports response data up to the SDK's fixed buffer size. */
    if (le > sizeof(sdkResponse.mDataOut)) {
        PICC_LOG("PICC APDU rejected: le=%lu exceeds SDK capacity=%lu",
                 (unsigned long)le,
                 (unsigned long)sizeof(sdkResponse.mDataOut));
        goto cleanup;
    }

    if (lc != 0u && request->dataIn == NULL) {
        PICC_LOG("PICC APDU rejected: NULL command data with nonzero lc");
        goto cleanup;
    }

    if (capacity != 0u && response->dataOut == NULL) {
        PICC_LOG(
            "PICC APDU rejected: NULL response buffer with nonzero capacity");
        goto cleanup;
    }

    /*
     * Reject an obviously insufficient destination before sending a command
     * that could change card state.
     */
    if (le > capacity) {
        PICC_LOG(
            "PICC APDU rejected: response capacity=%lu smaller than le=%lu",
            (unsigned long)capacity, (unsigned long)le);
        goto cleanup;
    }

    memcpy(sdkRequest.mCommand, request->command, sizeof(sdkRequest.mCommand));

    sdkRequest.mLc = lc;
    sdkRequest.mLe = le;

    if (lc != 0u) {
        memcpy(sdkRequest.mDataIn, request->dataIn, (size_t)lc);
    }

    PICC_LOG("PICC APDU: calling SDK");

    PICC_LOG("PICC APDU: opened=%d powered=%d "
             "polling=%d cardType=%d scanId=%lu",
             (int)self->opened, (int)self->powered, (int)self->pollingEnabled,
             (int)self->data.cardType, (unsigned long)self->scanId);

    PICC_LOG("PICC APDU: CLA=%02X INS=%02X P1=%02X P2=%02X "
             "Lc=%lu Le=%lu",
             sdkRequest.mCommand[0], sdkRequest.mCommand[1],
             sdkRequest.mCommand[2], sdkRequest.mCommand[3],
             (unsigned long)sdkRequest.mLc, (unsigned long)sdkRequest.mLe);

    sdkResult = sdkPiccExchangeApdu(&sdkRequest, &sdkResponse);
    error     = translateSdkErr(sdkResult);

    PICC_LOG("PICC APDU: SDK returned result=%ld error=%d", (long)sdkResult,
             (int)error);

    if (error != PICC_ERR_OK) {
        goto cleanup;
    }

    /* Validate SDK metadata before accessing/copying response bytes. */
    if (sdkResponse.mLenOut > sizeof(sdkResponse.mDataOut)) {
        PICC_LOG("PICC APDU failed: invalid SDK response length=%lu max=%lu",
                 (unsigned long)sdkResponse.mLenOut,
                 (unsigned long)sizeof(sdkResponse.mDataOut));
        error = PICC_ERR_IO;
        goto cleanup;
    }

    if (sdkResponse.mSWA > 0xFFu || sdkResponse.mSWB > 0xFFu) {
        PICC_LOG("PICC APDU failed: SDK status value exceeds one byte");
        error = PICC_ERR_IO;
        goto cleanup;
    }

    if (sdkResponse.mLenOut > capacity) {
        PICC_LOG("PICC APDU failed: response length=%lu exceeds capacity=%lu; "
                 "command already exchanged",
                 (unsigned long)sdkResponse.mLenOut, (unsigned long)capacity);

        /* Existing PiccErr_t has no dedicated capacity error. */
        error = PICC_ERR_BAD_PARAMETER;
        goto cleanup;
    }

    if (sdkResponse.mLenOut != 0u) {
        memcpy(response->dataOut, sdkResponse.mDataOut,
               (size_t)sdkResponse.mLenOut);
    }

    *response->lenOut = sdkResponse.mLenOut;
    *response->swa    = sdkResponse.mSWA;
    *response->swb    = sdkResponse.mSWB;

    PICC_LOG("PICC APDU: completed length=%lu SW1=%02X SW2=%02X",
             (unsigned long)sdkResponse.mLenOut, (unsigned int)sdkResponse.mSWA,
             (unsigned int)sdkResponse.mSWB);

cleanup:
    wipeApduBuffer(&sdkRequest, sizeof(sdkRequest));
    wipeApduBuffer(&sdkResponse, sizeof(sdkResponse));

    self->error = error;

    PICC_LOG("PICC APDU: finished error=%d", (int)error);
    return error;
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
    self->base.vtable.activate     = activate;
}

#endif