#ifndef TMS_H_
#define TMS_H_

#include <stdbool.h>
#include <stdint.h>

#include "oop.h"
#include "tmsBackend.h"

/*
 * Vendor-independent Terminal Management System (TMS) manager.
 *
 * The manager does not implement download protocols, network transports,
 * persistent storage, or package installation. A backend owns those details.
 *
 * Threading contract: all manager calls and backend notifications must be
 * serialized onto the same application execution context. Worker-thread
 * backends must post their notifications to that context before invoking the
 * callback. This module does not provide locking or event marshaling.
 */

typedef struct {
    TmsBackend backend;
    bool allowBlocking;  /* Explicit opt-in for synchronous vendor SDKs. */
    TmsStatusCb onProgress;
    TmsStatusCb onDone;
    void* userData;
} TmsCfg;

/*
 * OOP-style singleton, following TxnMgr's public calling convention.
 * All function pointers operate on the singleton returned by tmsMgr().
 */
OOP_CLASS(TmsMgr) {
    TmsCfg config;     /* Copy of the accepted run's configuration. */
    TmsStatus status;
    uint64_t lastOperationId;
    bool active;

    /*
     * Return true when a request is ACCEPTED; completion may be synchronous.
     * Rejected requests are reported via that request's onDone callback with
     * TMS_RESULT_REJECT, without altering an already active operation.
     */
    bool (*run)(const TmsCfg* cfg, TmsTrigger trigger);

    /* Optional cooperative backend pump. Safe if no run is active. */
    void (*process)(void);

    /* True when cancellation request was accepted; completion may be later. */
    bool (*cancel)(void);

    /* Returns false if busy; does not cancel an operation. */
    bool (*reset)(void);

    bool (*isBusy)(void);
    TmsStatus (*getStatus)(void);
};

TmsMgr* tmsMgr(void);

#endif /* TMS_H_ */
