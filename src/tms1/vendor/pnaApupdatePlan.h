#ifndef PNA_APUPDATE_PLAN_H_
#define PNA_APUPDATE_PLAN_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pnaApupdate.h"

/* Legacy device updater consumes one container at this exact path. */
#define PNA_APUPDATE_INSTALL_PATH "/mtd2/apupdate"
#define PNA_APUPDATE_PLAN_PATH_SIZE 128u
#define PNA_APUPDATE_PLAN_COPY_READ_MAX 1024u

/* Descriptive plan ONLY. No callback, File HAL write, update flag or reboot
 * is exposed. A future production adapter must re-check all facts and
 * enforce trusted publisher authentication before preparing this path.
 */
typedef struct {
    char source[PNA_APUPDATE_PLAN_PATH_SIZE];
    char destination[PNA_APUPDATE_PLAN_PATH_SIZE];
    uint64_t packageBytes;           /* minimum data bytes required at destination */
    uint32_t headerRewriteBytes;     /* reserved first 4096 bytes */
    uint32_t headerPreviewTextBytes; /* actual INI text before zero padding */
    uint32_t headerPreviewCrc32;     /* CRC32 of ALL 4096 bytes, standard finalized */
    uint8_t embeddedFileCount;
    bool sourceReverified;
    bool embeddedCrcsVerified;
    bool destinationSpaceConfirmed;  /* false; no capacity query in supplied HAL */
    bool destinationOwnershipConfirmed; /* false; never touch live /mtd2 path */
    bool publisherAuthenticated;     /* false; legacy MD5 is not a signature */
    bool installationAuthorized;     /* false */
    bool writesEnabled;              /* false */
    bool flagEnabled;                /* false */
    bool rebootEnabled;              /* false */
} PnaApupdatePreparePlan;

/* Caller MUST first reverify source against the managed journal, parse the
 * legacy container, verify all embedded CRCs and build the INI preview.
 * This function has no filesystem or network side effects. It rejects
 * inconsistent input and fills a non-executable dry-run plan.
 */
bool pnaApupdatePlanBuild(const PnaApupdateInfo* info,
                          const char* locallyGeneratedSourcePath,
                          const uint8_t* preparedHeader4096,
                          size_t previewTextBytes,
                          bool sourceReverified,
                          bool embeddedCrcsVerified,
                          PnaApupdatePreparePlan* out);

#endif
