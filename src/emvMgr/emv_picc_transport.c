#include "emv_picc_transport.h"
#include "emvMgr.h"
#include <string.h>

/*
 * Map one transport command onto the supplied pointer-based PICC HAL.
 * IMPORTANT: Audit the concrete SDK bridge for mLenOut capacity semantics.
 * The HAL must never write more than response->capacity bytes; the public
 * ApduResp_t has no explicit capacity member, so that contract must be
 * implemented/enforced by the device's exchangeApdu method.
 */
static int picc_exchange(void* context, const EmvApduCommand* command,
                         EmvApduResponse* response) {
    EmvPiccAdapter* adapter = (EmvPiccAdapter*)context;
    uint8_t         header[4]; /* CLA, INS, P1, P2. */
    uint8_t
        body[EMV_MAX_APDU_DATA]; /* Non-const input for SDK compatibility. */
    uint32_t lc, le, len_out, swa, swb; /* SDK's pointer-based length fields. */
    ApduReq_t  req;
    ApduResp_t resp;
    PiccErr_t  err;
    if (adapter == NULL || adapter->device == NULL || command == NULL ||
        response == NULL || response->data == NULL ||
        command->data_len > sizeof(body) ||
        (command->data_len != 0u && command->data == NULL) ||
        response->capacity == 0u || response->capacity > UINT32_MAX ||
        (command->has_le && (command->le == 0u || command->le > 256u))) {
        return -1;
    }
    header[0] = command->cla;
    header[1] = command->ins;
    header[2] = command->p1;
    header[3] = command->p2;
    memset(body, 0, sizeof(body));
    if (command->data_len != 0u) {
        memcpy(body, command->data, command->data_len);
    }
    lc = (uint32_t)command->data_len;
    le = command->has_le ? ((command->le == 256u) ? 0u : command->le) : 0u;
    /* Some SDKs treat lenOut as OUTPUT ONLY; verify your concrete HAL. */
    len_out      = (uint32_t)response->capacity;
    swa          = 0u;
    swb          = 0u;
    req.command  = header;
    req.lc       = &lc;
    req.dataIn   = body;
    req.le       = &le;
    resp.lenOut  = &len_out;
    resp.dataOut = response->data;
    resp.swa     = &swa;
    resp.swb     = &swb;
    err          = OOP_CALL(adapter->device, exchangeApdu, &req, &resp);
    memset(body, 0, sizeof(body));
    if (err == PICC_ERR_TIMEOUT) {
        return -2;
    }
    if (err != PICC_ERR_OK || len_out > response->capacity || swa > UINT8_MAX ||
        swb > UINT8_MAX) {
        return -1;
    }
    response->length = (size_t)len_out;
    response->sw1    = (uint8_t)swa;
    response->sw2    = (uint8_t)swb;
    return 0;
}

EmvTransport emvPiccTransport(EmvPiccAdapter* adapter) {
    EmvTransport result;
    result.context  = adapter;
    result.exchange = picc_exchange;
    return result;
}
