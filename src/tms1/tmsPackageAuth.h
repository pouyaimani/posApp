#ifndef TMS_PACKAGE_AUTH_H_
#define TMS_PACKAGE_AUTH_H_

#include <stdbool.h>
#include "tmsInstaller.h"

/* Authentication is NOT supplied by the legacy MD5 manifest. A platform
 * implementation must validate an independently signed package/manifest,
 * a trusted publisher key/chain, complete file set, target compatibility and
 * anti-rollback policy. No such implementation is bundled here.
 * This callback is synchronous and read-only. Never retain plan pointers.
 */
typedef enum {
    TMS_PACKAGE_AUTH_UNAVAILABLE = 0, /* no configured verifier */
    TMS_PACKAGE_AUTH_REJECTED = 1,    /* unsigned/untrusted/incompatible */
    TMS_PACKAGE_AUTH_TRUSTED = 2,     /* REAL device trusted verification */
    TMS_PACKAGE_AUTH_SIMULATED = 3,   /* diagnostics ONLY; NEVER trusted */
    TMS_PACKAGE_AUTH_ERROR = 4       /* provider I/O/internal failure */
} TmsPackageAuthResult;

typedef TmsPackageAuthResult (*TmsPackageAuthVerify)(
    void* context, const TmsInstallerPlan* plan,
    TmsInstallerDescribeFile describe, void* fileContext);

typedef struct {
    void* context;
    TmsPackageAuthVerify verify;
} TmsPackageAuthenticator;

/* Fail-closed gate: rejects absent/malformed providers and plans. The result
 * TRUSTED is an assertion by the DEVICE-OWNED provider, not this module.
 * SIMULATED is never upgraded to TRUSTED. No install authorization is written.
 */
TmsPackageAuthResult tmsPackageAuthCheck(
    const TmsPackageAuthenticator* authenticator,
    const TmsInstallerPlan* plan,
    TmsInstallerDescribeFile describe,
    void* fileContext);

#endif
