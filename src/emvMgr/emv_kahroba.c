#include "emvMgr.h"
#include "emv_picc_transport.h"

static const uint8_t KAHROBA_AID[] = {0xA0, 0x00, 0x00, 0x08, 0x03, 0x00, 0x01};

static EmvPiccAdapter adapter;

EmvMgrError startKahrobaEmv(Picc* reader) {
    EmvMgrError err;

    if (reader == NULL)
        return EMV_MGR_ERR_ARGUMENT;

    adapter.device = reader;

    emvMgr()->reset();

    err = emvMgr()->init(emvPiccTransport(&adapter));

    if (err != EMV_MGR_OK)
        return err;

    /* After successful detection of a compatible card. */
    err = OOP_CALL(picc(), activate);

    if (err != PICC_ERR_OK) {
        LOG_TRACE("Kahroba: PICC activation failed error=%d", (int)err);
        /* Release the retained reader and report failure. */
        return err;
    }

    return emvMgr()->selectPpse(KAHROBA_AID, sizeof(KAHROBA_AID));
}