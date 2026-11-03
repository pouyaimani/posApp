#include "tmsInstaller.h"
#include <string.h>

static bool isValidFile(const TmsInstallerFile* f) {
    if (!f || !f->path[0] ||
        !memchr(f->path, '\0', sizeof(f->path))) return false;
    return true;
}

bool tmsInstallerDryRun(const TmsInstaller* installer,
                        const TmsInstallerPlan* plan,
                        TmsInstallerDescribeFile describe,
                        void* fileContext,
                        TmsInstallerDryRunReport* report) {
    if (report) memset(report, 0, sizeof(*report));
    if (!installer || !installer->preflight || !plan || !describe || !report ||
        !plan->stagingPrefix || !plan->stagingPrefix[0] ||
        !plan->contentsReverified || !plan->dryRunOnly ||
        plan->installationAuthorized ||
        plan->fileCount == 0u || plan->fileCount > TMS_INSTALLER_MAX_FILES)
        return false;

    /* Validate the complete description before calling the device adapter.
     * No file content is read here; the backend has rehashed every file first.
     */
    uint64_t sum = 0u;
    for (size_t i = 0u; i < plan->fileCount; ++i) {
        TmsInstallerFile file;
        memset(&file, 0, sizeof(file));
        if (!describe(fileContext, i, &file) || !isValidFile(&file) ||
            UINT64_MAX - sum < file.size)
            return false;
        sum += file.size;
    }
    if (sum != plan->totalBytes) return false;

    for (size_t i = 0u; i < plan->fileCount; ++i) {
        TmsInstallerFile file;
        memset(&file, 0, sizeof(file));
        if (!describe(fileContext, i, &file) || !isValidFile(&file) ||
            !installer->preflight(installer->context, plan, i, &file))
            return false;
    }

    report->checkedFiles = plan->fileCount;
    report->checkedBytes = sum;
    report->installationAuthorized = false;
    return true;
}
