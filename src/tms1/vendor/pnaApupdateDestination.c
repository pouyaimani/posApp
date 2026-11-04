#include "pnaApupdateDestination.h"

#include <limits.h>
#include <string.h>

PnaApupdateDestinationStatus pnaApupdateDestinationInspect(
    const PnaApupdatePreparePlan* plan, bool fileHalSaysExists,
    PnaApupdateDestinationProbe probe, void* probeContext,
    PnaApupdateDestinationReport* out) {
    if (!out) return PNA_DEST_UNVERIFIED;
    memset(out, 0, sizeof(*out));
    out->status = PNA_DEST_UNVERIFIED;
    if (!plan || memcmp(plan->destination, PNA_APUPDATE_INSTALL_PATH,
                         sizeof(PNA_APUPDATE_INSTALL_PATH)) != 0 ||
        plan->source[0] == '\0' || plan->embeddedFileCount == 0u ||
        plan->packageBytes <= PNA_APUPDATE_HEADER_LEN ||
        plan->headerRewriteBytes != PNA_APUPDATE_HEADER_LEN ||
        !plan->sourceReverified || !plan->embeddedCrcsVerified ||
        plan->publisherAuthenticated || plan->installationAuthorized ||
        plan->writesEnabled || plan->flagEnabled || plan->rebootEnabled ||
        plan->packageBytes > UINT64_MAX - PNA_APUPDATE_DEST_RESERVE_BYTES) {
        out->status = PNA_DEST_BLOCKED;
        return out->status;
    }

    out->requiredBytes = plan->packageBytes + PNA_APUPDATE_DEST_RESERVE_BYTES;
    out->existingDestinationObserved = fileHalSaysExists;
    if (fileHalSaysExists) {
        out->status = PNA_DEST_BLOCKED;
        return out->status;
    }
    if (!probe) return out->status;

    PnaApupdateDestinationEvidence evidence = {0};
    out->probeCalled = true;
    if (!probe(probeContext, plan->destination, out->requiredBytes, &evidence))
        return out->status;

    out->mountConfirmed = evidence.writableMountConfirmed;
    out->absenceConfirmed = evidence.destinationAbsentConfirmed;
    out->updateFlagClearConfirmed = evidence.updateFlagClearConfirmed;
    out->availableBytes = evidence.availableBytes;
    if (evidence.availableBytesKnown) {
        out->spaceConfirmed = evidence.availableBytes >= out->requiredBytes;
        if (!out->spaceConfirmed) {
            out->status = PNA_DEST_BLOCKED;
            return out->status;
        }
    }
    if (out->mountConfirmed && out->absenceConfirmed &&
        out->updateFlagClearConfirmed && out->spaceConfirmed)
        out->status = PNA_DEST_CONFIRMED;
    return out->status;
}
