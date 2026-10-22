#ifndef EMV_MGR_H
#define EMV_MGR_H

#include "emv_kahroba.h"
#include "emv_transport.h"

#ifndef EMV_MAX_APDU_DATA
#define EMV_MAX_APDU_DATA 255u
#endif
#ifndef EMV_MAX_APDU_RESPONSE
#define EMV_MAX_APDU_RESPONSE 512u
#endif
#ifndef EMV_MAX_AFL_BYTES
#define EMV_MAX_AFL_BYTES 64u
#endif
#ifndef EMV_MAX_READ_RECORDS
#define EMV_MAX_READ_RECORDS 24u
#endif

typedef enum {
    EMV_OUTCOME_NONE = 0,        /* Processing is not finished. */
    EMV_OUTCOME_ONLINE_REQUIRED, /* ARQC requested; host may authorize. */
    EMV_OUTCOME_DECLINED,        /* Application cryptogram AAC. */
    EMV_OUTCOME_UNSUPPORTED,     /* TC/offline or flow needs a full kernel. */
    EMV_OUTCOME_FAILED,          /* Transport/protocol/validation error. */
    EMV_OUTCOME_CANCELLED        /* Session was explicitly cancelled. */
} EmvOutcome;

typedef enum {
    EMV_STATE_IDLE = 0,
    EMV_STATE_SELECT_PPSE,
    EMV_STATE_SELECT_AID,
    EMV_STATE_GPO,
    EMV_STATE_READ_RECORD,
    EMV_STATE_GENERATE_AC,
    EMV_STATE_FINISHED
} EmvState;

typedef enum {
    EMV_STEP_PROGRESS = 0, /* Exactly one APDU processed successfully. */
    EMV_STEP_FINISHED,     /* Result available, no further APDUs. */
    EMV_STEP_REJECTED      /* Invalid state or arguments. */
} EmvStepResult;

typedef struct {
    uint32_t transaction_id;      /* Supplied by txnMgr for stale-event filtering. */
    uint32_t session_id;          /* Supplied by nfcMgr, unique for active scan. */
    bool select_ppse_first;        /* Verify Kahroba AID is in the PPSE. */
    const EmvTagDb *terminal_tags;/* Precomputed terminal values, copied at begin. */
} EmvStartParams;

typedef struct {
    uint32_t transaction_id;   /* Stable transaction correlation identifier. */
    uint32_t session_id;       /* RF session identifier. */
    EmvOutcome outcome;       /* NOT the host's authorization decision. */
    EmvError error;           /* Error explaining a failed/unsupported outcome. */
    EmvTagDb tags;            /* Owned copy of card and terminal EMV tags. */
} EmvResult;

/** Small POD event safe to copy into the application's event queue. */
typedef struct {
    uint32_t transaction_id; /**< Correlates with txnMgr's active transaction. */
    uint32_t session_id;     /**< Correlates with nfcMgr's active scan/session. */
    EmvOutcome outcome;      /**< ONLINE_REQUIRED is NOT a transaction approval. */
    EmvError error;          /**< Transport/protocol failure detail. */
} EmvCompletedEvent;

/** Called synchronously on the worker running emvMgrStep().
 * Copy the event by value into the real event queue; never retain its pointer.
 * Callback must not reset/reenter the manager or block on its own worker.
 */
typedef void (*EmvEventFn)(void *context, const EmvCompletedEvent *event);

typedef struct {
    EmvTransport transport;        /* Exchange callback and device context. */
    EmvStartParams request;       /* Configuration copied at begin. */
    EmvEventFn on_completed;      /* Optional terminal-event callback. */
    void *event_context;          /* Opaque callback context owned by caller. */
    EmvResult result;             /* Session-owned result, including tag arena. */
    EmvState state;               /* Current cooperative processing state. */
    uint8_t afl[EMV_MAX_AFL_BYTES];/* AFL copied from GPO response. */
    size_t afl_len;               /* Valid bytes of AFL. */
    size_t afl_pos;               /* Current AFL entry, 4 bytes each. */
    uint8_t record_no;           /* Current record in the AFL SFI. */
    unsigned records_read;        /* APDUs performed for AFL records. */
    bool active;                  /* True until completion/cancel/reset. */
    uint8_t apdu_data[EMV_MAX_APDU_DATA]; /* Scratch command payload. */
    uint8_t response[EMV_MAX_APDU_RESPONSE]; /* Scratch response payload. */
} EmvMgr;

/** Initialize an idle EMV manager with a caller-supplied transport adapter. */
EmvError emvMgrInit(EmvMgr *mgr, EmvTransport transport);

/** Configure completion notification; no callback during initial setup. */
EmvError emvMgrSetEventCallback(EmvMgr *mgr, EmvEventFn callback,
                                void *context);

/** Begin a session only after nfcMgr has activated a card and stopped polling. */
EmvError emvMgrBegin(EmvMgr *mgr, const EmvStartParams *params);

/** Run at most one blocking APDU exchange per call; call from an NFC worker/task. */
EmvStepResult emvMgrStep(EmvMgr *mgr);

/**
 * Run the entire Kahroba APDU sequence synchronously.
 *
 * Preconditions:
 * - The contactless card is activated and PICC polling is stopped.
 * - Caller exclusively owns the reader for the entire call.
 * - Call on a transaction task, NOT the GUI/event-loop thread.
 * - HAL APDU exchanges must have finite timeouts and bounded responses.
 *
 * On return, outcome is set to the card/kernel processing outcome; an
 * ONLINE_REQUIRED result is NOT an approved financial transaction.
 *
 * EMV_OK means a terminal outcome was reached; even DECLINED may return
 * EMV_OK. Callers MUST examine *outcome before advancing txnMgr.
 * Result tag memory belongs to mgr until emvMgrReset().
 */
EmvError emvMgrRun(EmvMgr *mgr, const EmvStartParams *params,
                   EmvOutcome *outcome);

/** Return current terminal outcome. Intermediate states return NONE. */
EmvOutcome emvMgrOutcome(const EmvMgr *mgr);

/** Read-only completed result, valid until cancel/reset/new begin. */
const EmvResult *emvMgrResultView(const EmvMgr *mgr);

/** Copy a completed result (do NOT copy partial/failed data into TxnData). */
EmvError emvMgrCopyResult(const EmvMgr *mgr, EmvResult *destination);

/** Abort a session; must not race with an in-flight exchange. */
void emvMgrCancel(EmvMgr *mgr);

/** Securely erase session data; call before releasing hardware/card ownership. */
void emvMgrReset(EmvMgr *mgr);

#endif
