#ifndef INPUT_MGR_H_
#define INPUT_MGR_H_

#include <stdint.h>
#include "state.h"
#include "inputCtrl.h"
#include "timeout.h"

/** Optional adapter context for inputComplete(). Keep alive until completion.
 */
typedef struct {
    State* onFinish; /**< Destination after successful input; NULL skips
                        navigation. */
    State*
        onCancel; /**< Destination after cancellation; NULL skips navigation. */
    CallBack_t cb; /**< Optional notification, invoked after navigation. */
    void*      userData; /**< Borrowed argument passed to cb. */
} InputFinishCtx;

/**
 * Singleton input-screen coordinator. Use only from the UI/state-machine
 * thread. Public data fields are observations: callers must not overwrite them.
 * The controller owns input/finput; these pointers are NOT result copies.
 */
typedef struct {
    State* state; /**< Owned state, allocated once by inmgr(). */
    InputController*
        ctrl; /**< Borrowed keypadCtrl or pedCtrl for the last request. */

    /**
     * Reset input, copy cfg, configure timeout, and request state entry.
     * cfg itself may be temporary; cfg.title/info/cbData are borrowed.
     * Rejects invalid configurations and an already active/entered session.
     * Rejections are traced; this legacy void API cannot return an error.
     * Clears previous commit destinations. Call commit() after run().
     * Completion may be synchronous if provider startup fails.
     */
    void (*run)(InputCfg* cfg, InPutComplete cb, void* cbData);

    /** Set raw text (NULL means empty), bounded to buffer/configured capacity.
     * Refreshes displayed text for an active keypad request. PED display is
     * controlled by hardware events; this does not preload a hardware PIN.
     */
    void (*set)(InputType type, const char* in);

    /** Compatibility alias of set(); does not persist defaults between runs. */
    void (*setDefault)(InputType type, const char* in);

    /**
     * Register optional output buffers for successful KEYPAD completion only.
     * len is each destination's capacity INCLUDING the terminating NUL.
     * Destinations must remain writable/alive until completion, must not alias
     * controller buffers, and must not overlap each other. Small buffers
     * truncate. Cancellation and PED completion leave destinations unchanged.
     */
    void (*commit)(char* oraw, char* oformated, uint16_t len);

    /** Clear raw/formatted buffers and invoke the controller reset hook, if
     * any. Does not restart or clear an active hardware PED PIN-entry
     * operation.
     */
    void (*reset)(InputType type);

    char* input;  /**< Borrowed raw buffer; changes when controller input
                     changes. */
    char* finput; /**< Borrowed formatted buffer; PED content is a display mask.
                   */

    char* oraw; /**< Borrowed raw destination; NULL disables raw copying. */
    char* oformated; /**< Borrowed formatted destination; legacy spelling
                        retained. */
    uint16_t
        outlen; /**< Capacity shared by both destinations, including NUL. */

    InPutComplete
          cb;     /**< Current terminal callback, detached before invocation. */
    void* cbData; /**< Borrowed callback context. */
} InputMgr;

/** Initialize once and return the singleton; NULL if state allocation fails.
 * Call only after the display is ready, from the UI thread. */
InputMgr* inmgr(void);

/** Application-owned preset; its definition remains in your existing code. */
extern InputCfg inCfgEnterPin;

/** Convenience context; do not overwrite while a request uses it. */
extern InputFinishCtx inFinishCtx;

/** Navigate on FINISHED/CANCELED, then invoke ctx->cb. NULL context is ignored.
 */
void inputComplete(InputResult result, void* userData);

#endif
