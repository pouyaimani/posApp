#include "tmsPackageAuth.h"
#include <stdint.h>
#include <string.h>

TmsPackageAuthResult tmsPackageAuthCheck(
    const TmsPackageAuthenticator* authenticator,
    const TmsInstallerPlan* plan,
    TmsInstallerDescribeFile describe,
    void* fileContext) {
    if (!plan || !describe || !plan->stagingPrefix ||
        !plan->stagingPrefix[0] || !plan->contentsReverified ||
        !plan->dryRunOnly || plan->installationAuthorized ||
        plan->publisherAuthenticated || plan->fileCount == 0u ||
        plan->fileCount > TMS_INSTALLER_MAX_FILES)
        return TMS_PACKAGE_AUTH_REJECTED;

    /* Verify stable descriptions and aggregate size before trusted callback. */
    uint64_t sum = 0u;
    for (size_t i = 0u; i < plan->fileCount; ++i) {
        TmsInstallerFile f;
        memset(&f, 0, sizeof(f));
        if (!describe(fileContext, i, &f) || !f.path[0] ||
            !memchr(f.path, '\0', sizeof(f.path)) ||
            UINT64_MAX - sum < f.size)
            return TMS_PACKAGE_AUTH_REJECTED;
        sum += f.size;
    }
    if (sum != plan->totalBytes) return TMS_PACKAGE_AUTH_REJECTED;
    if (!authenticator || !authenticator->verify)
        return TMS_PACKAGE_AUTH_UNAVAILABLE;

    TmsPackageAuthResult result = authenticator->verify(
        authenticator->context, plan, describe, fileContext);
    switch (result) {
    case TMS_PACKAGE_AUTH_TRUSTED:
    case TMS_PACKAGE_AUTH_SIMULATED:
    case TMS_PACKAGE_AUTH_REJECTED:
    case TMS_PACKAGE_AUTH_UNAVAILABLE:
    case TMS_PACKAGE_AUTH_ERROR:
        return result;
    default:
        return TMS_PACKAGE_AUTH_ERROR;
    }
}
