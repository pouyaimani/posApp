#ifndef PNA_APUPDATE_DESTINATION_H_
#define PNA_APUPDATE_DESTINATION_H_

#include <stdbool.h>
#include <stdint.h>
#include "pnaApupdatePlan.h"

/* Extra filesystem allowance above the exact package size. This is a
 * diagnostic policy, NOT a filesystem guarantee; platform may require more. */
#define PNA_APUPDATE_DEST_RESERVE_BYTES UINT64_C(65536)

/* All fields MUST be populated from READ-ONLY, platform-specific queries.
 * A false value means unknown/failed; never infer mount/free-space state
 * from File HAL's exists() == false. Do not create probe files.
 * destinationAbsentConfirmed must be independently checked on the same
 * mounted filesystem (e.g. stat with a distinguishable ENOENT result).
 */
typedef struct {
    bool writableMountConfirmed;
    bool destinationAbsentConfirmed;
    bool updateFlagClearConfirmed;
    bool availableBytesKnown;
    uint64_t availableBytes;
} PnaApupdateDestinationEvidence;

/* Probe is a READ-ONLY caller-supplied platform adapter; never install,
 * write, delete, set an update flag or reboot from this callback.
 * Return false if the SDK cannot provide authoritative information. */
typedef bool (*PnaApupdateDestinationProbe)(
    void* context, const char* destination, uint64_t requiredBytes,
    PnaApupdateDestinationEvidence* evidence);

typedef enum {
    PNA_DEST_UNVERIFIED = 0, /* Safe to report, NOT ready for any write. */
    PNA_DEST_BLOCKED = 1,    /* Existing target or known capacity/flag issue. */
    PNA_DEST_CONFIRMED = 2   /* Read-only prerequisites confirmed for this instant. */
} PnaApupdateDestinationStatus;

typedef struct {
    PnaApupdateDestinationStatus status;
    uint64_t requiredBytes;
    uint64_t availableBytes;
    bool existingDestinationObserved;
    bool probeCalled;
    bool mountConfirmed;
    bool absenceConfirmed;
    bool spaceConfirmed;
    bool updateFlagClearConfirmed;
    /* Always false: authentication and atomic replacement not implemented. */
    bool publisherAuthenticated;
    bool installationAuthorized;
} PnaApupdateDestinationReport;

/* Pure policy check. The caller makes one read-only File HAL exists() query
 * and passes its result. A negative exists() result is NOT proof of absence.
 * On success of the full probe, the status means ONLY that the observed
 * destination preconditions passed, not authorization to install. */
PnaApupdateDestinationStatus pnaApupdateDestinationInspect(
    const PnaApupdatePreparePlan* plan, bool fileHalSaysExists,
    PnaApupdateDestinationProbe probe, void* probeContext,
    PnaApupdateDestinationReport* out);

#endif
