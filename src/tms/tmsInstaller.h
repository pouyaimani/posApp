#ifndef TMS_INSTALLER_H_
#define TMS_INSTALLER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Vendor-independent, READ-ONLY installer handoff contract.
 * Stage 10 preflight and Stage 11 authentication gate have deliberately NO
 * install/activate/reboot entry point. An accepted dry-run proves only that
 * the adapter inspected the plan; no executable may be installed.
 */
#define TMS_INSTALLER_PATH_SIZE 128u
#define TMS_INSTALLER_MD5_SIZE  16u
#define TMS_INSTALLER_MAX_FILES 40u

typedef struct {
    char path[TMS_INSTALLER_PATH_SIZE];  /* generated locally, NUL terminated */
    uint64_t size;
    uint8_t manifestMd5[TMS_INSTALLER_MD5_SIZE]; /* integrity, NOT a signature */
} TmsInstallerFile;

typedef struct {
    const char* stagingPrefix; /* stable for duration of synchronous call */
    size_t fileCount;
    uint64_t totalBytes;
    bool contentsReverified; /* fresh per-byte hash against saved manifest */
    bool publisherAuthenticated; /* true ONLY if a real trusted verifier succeeded */
    bool installationAuthorized; /* MUST remain false in Stages 10 and 11 */
    bool dryRunOnly; /* Stages 10 and 11 MUST be true */
} TmsInstallerPlan;

/* Enumerate a file by index. No whole-file list or large stack buffer needed. */
typedef bool (*TmsInstallerDescribeFile)(void* context, size_t index,
                                          TmsInstallerFile* output);

/* Read-only device adapter callback, called once for each file.
 * Must NOT write files, invoke vendor installation APIs, reboot, or retain
 * plan/file pointers. Reject incompatible packages by returning false.
 */
typedef bool (*TmsInstallerPreflight)(void* context,
                                       const TmsInstallerPlan* plan,
                                       size_t index,
                                       const TmsInstallerFile* file);

typedef struct {
    void* context;
    TmsInstallerPreflight preflight;
} TmsInstaller;

typedef struct {
    size_t checkedFiles;
    uint64_t checkedBytes;
    bool installationAuthorized; /* always false for dry-run */
} TmsInstallerDryRunReport;

/* Read-only validation + callback handoff. Performs no file/network I/O.
 * No installer activation is representable through this interface.
 * 'report' is zeroed on entry and populated only on success.
 */
bool tmsInstallerDryRun(const TmsInstaller* installer,
                        const TmsInstallerPlan* plan,
                        TmsInstallerDescribeFile describe,
                        void* fileContext,
                        TmsInstallerDryRunReport* report);

#endif
