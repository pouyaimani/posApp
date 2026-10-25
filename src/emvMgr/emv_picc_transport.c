#include "emv_picc_transport.h"

#include <stdint.h>
#include <string.h>

#include "logger.h"
#include "emvMgr.h"

#define EMV_MAX_APDU_DATA 255u

/** Erase temporary command data without an optimizable memset(). */
static void wipeCommandData(void* buffer, size_t length) {
    volatile uint8_t* p = (volatile uint8_t*)buffer;

    while (length != 0u) {
        *p++ = 0u;
        --length;
    }
}

/**
 * Exchange one short APDU through the PICC HAL.
 *
 * command->le is a decoded length:
 *   has_le == false: no response data requested.
 *   has_le == true:  le must be 1..256, including 256 unchanged.
 *
 * HAL response lenOut:
 *   Input:  destination capacity.
 *   Output: actual response-data length, excluding SW1/SW2.
 *
 * Returns:
 *    0: exchange succeeded; caller interprets SW1/SW2.
 *   -1: invalid arguments, HAL failure, or invalid response.
 *   -2: HAL timeout.
 *
 * The caller owns the active reader and must serialize PICC operations.
 */
static int picc_exchange(void* context, const EmvApduCommand* command,
                         EmvApduResponse* response) {
    EmvPiccAdapter* adapter = (EmvPiccAdapter*)context;

    uint8_t header[4];
    uint8_t body[EMV_MAX_APDU_DATA];

    uint32_t lc;
    uint32_t le;
    uint32_t len_out;
    uint32_t swa = 0u;
    uint32_t swb = 0u;

    ApduReq_t  req  = {0};
    ApduResp_t resp = {0};

    PiccErr_t err;

    /* Never leave previous response metadata visible after failure. */
    if (response != NULL) {
        response->length = 0u;
        response->sw1    = 0u;
        response->sw2    = 0u;
    }

    if (adapter == NULL || adapter->device == NULL || command == NULL ||
        response == NULL) {
        LOG_TRACE("EMV PICC rejected: missing adapter/device/command/response");
        return -1;
    }

    if (response->data == NULL || response->capacity == 0u ||
        response->capacity > UINT32_MAX) {
        LOG_TRACE("EMV PICC rejected: invalid response buffer/capacity");
        return -1;
    }

    /*
     * Both the local buffer and the supplied SDK request must fit the data.
     * This adapter handles short APDUs: Lc cannot exceed 255.
     */
    if (command->data_len > sizeof(body) || command->data_len > 255u ||
        (command->data_len != 0u && command->data == NULL)) {
        LOG_TRACE(
            "EMV PICC rejected: invalid command data length=%lu capacity=%lu",
            (unsigned long)command->data_len, (unsigned long)sizeof(body));
        return -1;
    }

    if (command->has_le && (command->le == 0u || command->le > 256u)) {
        LOG_TRACE("EMV PICC rejected: short APDU Le must be 1..256");
        return -1;
    }

    lc = (uint32_t)command->data_len;

    /* SDK takes the decoded length: preserve 256, do not encode it as zero. */
    le = command->has_le ? (uint32_t)command->le : 0u;

    if (le > response->capacity) {
        LOG_TRACE(
            "EMV PICC rejected: capacity=%lu smaller than expected length=%lu",
            (unsigned long)response->capacity, (unsigned long)le);
        return -1;
    }

    if (!adapter->device->opened || !adapter->device->powered) {
        LOG_TRACE("EMV PICC rejected: reader is not opened and powered");
        return -1;
    }

    header[0] = command->cla;
    header[1] = command->ins;
    header[2] = command->p1;
    header[3] = command->p2;

    if (lc != 0u) {
        memcpy(body, command->data, (size_t)lc);
    }

    len_out = (uint32_t)response->capacity;

    req.command = header;
    req.lc      = &lc;
    req.dataIn  = lc != 0u ? body : NULL;
    req.le      = &le;

    resp.lenOut  = &len_out;
    resp.dataOut = response->data;
    resp.swa     = &swa;
    resp.swb     = &swb;

    LOG_TRACE("EMV PICC: exchange begin lc=%lu le=%lu capacity=%lu",
              (unsigned long)lc, (unsigned long)le, (unsigned long)len_out);

    err = OOP_CALL(adapter->device, exchangeApdu, &req, &resp);

    wipeCommandData(body, sizeof(body));

    LOG_TRACE("EMV PICC: HAL returned error=%d length=%lu", (int)err,
              (unsigned long)len_out);

    if (err == PICC_ERR_TIMEOUT) {
        LOG_TRACE("EMV PICC: exchange timed out");
        return -2;
    }

    if (err != PICC_ERR_OK) {
        LOG_TRACE("EMV PICC: exchange failed error=%d", (int)err);
        return -1;
    }

    if (len_out > response->capacity || swa > UINT8_MAX || swb > UINT8_MAX) {
        LOG_TRACE("EMV PICC: invalid HAL response length=%lu capacity=%lu "
                  "sw1=%lu sw2=%lu",
                  (unsigned long)len_out, (unsigned long)response->capacity,
                  (unsigned long)swa, (unsigned long)swb);
        return -1;
    }

    response->length = (size_t)len_out;
    response->sw1    = (uint8_t)swa;
    response->sw2    = (uint8_t)swb;

    LOG_TRACE("EMV PICC: completed length=%lu SW1=%02X SW2=%02X",
              (unsigned long)response->length, (unsigned int)response->sw1,
              (unsigned int)response->sw2);

    return 0;
}

/** Bind an adapter; this does not open, power, or take ownership of the reader.
 */
EmvTransport emvPiccTransport(EmvPiccAdapter* adapter) {
    EmvTransport result = {0};

    result.context  = adapter;
    result.exchange = picc_exchange;

    return result;
}