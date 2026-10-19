#ifndef NFC_MGR_H_
#define NFC_MGR_H_

#include <stdbool.h>
#include "state.h"
#include "picc/picc.h"

/** Terminal outcome of a card-acquisition request, not of a payment. */
typedef enum {
    NFC_RES_DETECTED = 0,
    NFC_RES_CANCELED,
    NFC_RES_TIMEOUT,
    NFC_RES_ERROR
} NfcResult;

/** Callback snapshot. Copy it if needed after the callback returns. */
typedef struct {
    NfcResult  result; /**< Acquisition outcome. */
    PiccData_t card; /**< Value copy; the supplied HAL exposes cardType only. */
    PiccErr_t  error; /**< Primary operation error; OK on user cancellation. */
    PiccErr_t
        cleanupError; /**< close() error, independent of the primary outcome. */
    bool
        readerRetained; /**< Successful ownership handoff; caller must close. */
    bool readerOpen;    /**< HAL opened flag after completion, including close
                           failure. */
} NfcStatus;

typedef void (*NfcComplete)(const NfcStatus* status, void* userData);

/** Request options. All calls and event delivery must use the UI thread. */
typedef struct {
    const char* title;        /**< Borrowed title; NULL selects the default. */
    const char* info;         /**< Borrowed prompt; NULL selects the default. */
    const char* multipleInfo; /**< Borrowed multiple-card warning; NULL selects
                                 default. */
    PiccTechMask_t technologies; /**< Nonempty mask of PICC_TECH_A/B/FELICA. */
    uint32_t timeout; /**< Same units/zero semantics as State.setTimeOut. */
    bool     keepOpenOnSuccess; /**< Transfer powered reader to caller after
                                   detection. */
} NfcCfg;

typedef struct {
    State* state; /**< Manager-owned state. Do not overwrite its handlers. */

    /**
     * Copy cfg and request state entry. Returns false on invalid config, busy
     * manager, already-open reader, missing callback, or unresolved close
     * failure. Rejected requests never invoke cb. Accepted requests complete
     * once, except external state navigation which silently aborts (as in
     * inputMgr). cb may run before run returns if initialization/power-on
     * fails. cfg strings/context must remain alive until completion or external
     * exit.
     */
    bool (*run)(const NfcCfg* cfg, NfcComplete cb, void* userData);

    /** Cancel an accepted request. No effect after completion. */
    void (*cancel)(void);

    /** True while waiting for a card, including pending state entry. */
    bool (*isBusy)(void);

    /**
     * Retry a failed manager-owned close after leaving the NFC state.
     * Does not close a reader successfully handed to the caller.
     * Returns OK if this manager has no failed cleanup to retry.
     */
    PiccErr_t (*retryCleanup)(void);
} NfcMgr;

/** Initialize after display startup. Returns NULL if state allocation fails. */
NfcMgr* nfcmgr(void);

#endif
