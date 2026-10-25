#ifndef EMV_PICC_TRANSPORT_H
#define EMV_PICC_TRANSPORT_H

#include "emv_transport.h"
#include "picc/picc.h" /* Project HAL, supplied by the device port. */

/** Adapter borrows the reader; acquisition/transaction code owns its lifecycle.
 */
typedef struct {
    Picc* device; /* Active, powered, exclusively assigned; polling stopped. */
} EmvPiccAdapter;

/** Initialize adapter and return a device-independent transport interface. */
EmvTransport emvPiccTransport(EmvPiccAdapter* adapter);

#endif
