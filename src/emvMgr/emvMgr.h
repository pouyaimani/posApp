#ifndef EMV_MGR_H
#define EMV_MGR_H

#include <stddef.h>
#include <stdint.h>

#include "emv_transport.h"

typedef enum {
    EMV_MGR_OK = 0,
    EMV_MGR_ERR_ARGUMENT,
    EMV_MGR_ERR_STATE,
    EMV_MGR_ERR_TRANSPORT,
    EMV_MGR_ERR_TIMEOUT,
    EMV_MGR_ERR_CARD_STATUS,
    EMV_MGR_ERR_PROTOCOL,
    EMV_MGR_ERR_UNSUPPORTED
} EmvMgrError;

typedef enum {
    EMV_MGR_IDLE = 0,
    EMV_MGR_PPSE_SELECTED,
    EMV_MGR_FAILED
} EmvMgrState;

/**
 * Public EMV manager interface.
 *
 * The manager does not own the PICC reader.
 * The transaction/session coordinator owns reader cleanup.
 */
typedef struct {
    EmvMgrError (*init)(EmvTransport transport);

    EmvMgrError (*selectPpse)(const uint8_t* aid, size_t aidLen);

    EmvMgrState (*getState)(void);

    void (*reset)(void);
} EmvMgr;

/** Return the application-wide EMV manager singleton. */
EmvMgr* emvMgr(void);

#endif