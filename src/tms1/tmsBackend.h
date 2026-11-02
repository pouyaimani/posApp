#ifndef TMS_BACKEND_H_
#define TMS_BACKEND_H_

#include <stdbool.h>
#include <stdint.h>
#include "tmsTypes.h"

/*
 * Backends own vendor-specific network, protocol, staging and installation.
 * All notifications must occur on the manager's serialized execution context.
 * A backend must release its resources BEFORE emitting DONE.
 */
typedef enum {
    TMS_BACKEND_EVENT_PROGRESS = 0,
    TMS_BACKEND_EVENT_DONE
} TmsBackendEventType;

typedef struct {
    TmsBackendEventType type;
    TmsState state;
    uint8_t progress;
    uint16_t fileIndex;
    uint16_t fileCount;
    TmsResult result;  /* DONE only. */
    TmsError error;    /* DONE only. */
    int32_t backendCode;
} TmsBackendEvent;

typedef void (*TmsBackendNotify)(void* notifyContext, uint64_t operationId,
                                 const TmsBackendEvent* event);

typedef struct {
    /*
     * Accepted if true. DONE can be delivered synchronously inside start().
     * False without DONE is converted into BACKEND_START failure by manager.
     */
    bool (*start)(void* context, TmsTrigger trigger, uint64_t operationId,
                  TmsBackendNotify notify, void* notifyContext);
    /* Optional: cooperative nonblocking progress pump. */
    void (*process)(void* context);
    /* Optional: cancellation request; true does NOT imply completion. */
    bool (*cancel)(void* context, uint64_t operationId);
} TmsBackendOps;

#define TMS_BACKEND_FLAG_BLOCKING_START (1u << 0)

typedef struct {
    const TmsBackendOps* ops; /* Lifetime >= operation. */
    void* context;           /* Lifetime >= operation. */
    uint32_t flags;
} TmsBackend;

#endif
