#ifndef TMS_TYPES_H_
#define TMS_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

/* Vendor-independent operation trigger. */
typedef enum {
    TMS_TRIGGER_MANUAL = 0,
    TMS_TRIGGER_AUTO,
    TMS_TRIGGER_SCHEDULED
} TmsTrigger;

typedef enum {
    TMS_RESULT_NONE = 0,
    TMS_RESULT_REJECT,
    TMS_RESULT_RUNNING,
    TMS_RESULT_SUCCESS,
    TMS_RESULT_NO_UPDATE,
    TMS_RESULT_FAILURE,
    TMS_RESULT_CANCELED,
    /* Explicitly stopped at a diagnostic checkpoint; NOT an update success. */
    TMS_RESULT_DIAGNOSTIC
} TmsResult;

typedef enum {
    TMS_STATE_IDLE = 0,
    TMS_STATE_STARTING,
    TMS_STATE_CONNECTING,
    TMS_STATE_CHECKING,
    TMS_STATE_DOWNLOADING,
    TMS_STATE_VERIFYING,
    TMS_STATE_INSTALLING,
    TMS_STATE_COMPLETED,
    TMS_STATE_FAILED,
    TMS_STATE_CANCELED
} TmsState;

typedef enum {
    TMS_ERR_NONE = 0,
    TMS_ERR_NULL_CONFIG,
    TMS_ERR_INVALID_BACKEND,
    TMS_ERR_BUSY,
    TMS_ERR_BLOCKING_NOT_ALLOWED,
    TMS_ERR_OPERATION_ID_EXHAUSTED,
    TMS_ERR_BACKEND_START,
    TMS_ERR_INVALID_EVENT,
    TMS_ERR_COMMUNICATION,
    TMS_ERR_TIMEOUT,
    TMS_ERR_PROTOCOL,
    TMS_ERR_STORAGE,
    TMS_ERR_INTEGRITY,
    TMS_ERR_INSTALLATION,
    TMS_ERR_BACKEND_FAILURE
} TmsError;

typedef struct {
    uint64_t operationId; /* Unique for each accepted run. */
    TmsTrigger trigger;
    TmsResult result;
    TmsState state;
    TmsError error;
    int32_t backendCode; /* Backend diagnostic (not standardized). */
    uint8_t progress;     /* Overall 0..100. */
    uint16_t fileIndex;   /* 1-based, 0 if unknown. */
    uint16_t fileCount;
    bool cancelRequested;
} TmsStatus;

typedef void (*TmsStatusCb)(const TmsStatus* status, void* userData);

#endif
