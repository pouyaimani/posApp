#ifndef EMV_TRANSPORT_H
#define EMV_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* This interface does not reference any vendor SDK or device type. */
typedef struct {
    uint8_t cla, ins, p1, p2;  /* ISO 7816 command header. */
    const uint8_t *data;       /* Optional request data, borrowed for call only. */
    size_t data_len;           /* Short APDU Lc (at most 255 bytes). */
    bool has_le;               /* Whether to ask for a response. */
    uint16_t le;               /* 1..256, 256 maps to short APDU Le=0. */
} EmvApduCommand;

typedef struct {
    uint8_t *data;        /* Response buffer owned by caller. */
    size_t capacity;      /* Hard response capacity. */
    size_t length;        /* Actual returned data length (excluding SW1/SW2). */
    uint8_t sw1, sw2;    /* Card status words. */
} EmvApduResponse;

/* Return 0 on success, -2 for a timeout, -1 (or other nonzero) for IO. */
typedef int (*EmvExchangeFn)(void *context, const EmvApduCommand *command,
                             EmvApduResponse *response);

typedef struct {
    void *context;           /* Opaque pointer, valid through the entire session. */
    EmvExchangeFn exchange;  /* Performs one synchronous APDU exchange. */
} EmvTransport;

#endif
