#ifndef EMV_PICC_TRANSPORT_H
#define EMV_PICC_TRANSPORT_H

#include "emv_transport.h"
#include "picc/picc.h" /* Project HAL, supplied by the device port. */

/** Context for the adapter; Picc is owned by nfcMgr (not emvMgr). */
typedef struct {
    Picc* device; /* Active, powered and exclusively assigned PICC instance. */
} EmvPiccAdapter;

/** Initialize adapter and return a device-independent transport interface. */
EmvTransport emvPiccTransport(EmvPiccAdapter* adapter);

#endif
