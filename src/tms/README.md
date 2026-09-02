# Async TMS client

Copy the `.h` and `.c` files into your project and add the directory to the
include path. The implementation replaces the old global `HttpContext` and
`LibTransmit` calls with `HttpFlow`/`Nth`.

## Modules

- `tmsHttpClient`: reusable JSON HTTP wrapper.
- `tmsJson`: request builders and response parsers.
- `tmsApiClient`: asynchronous TMS endpoints.
- `tmsDownloader`: bounded HTTP Range downloader.
- `tmsInstaller`: CRC32, APUPDATE header, switch flag, and reboot adapter.
- `tmsUpgrade`: end-to-end upgrade state machine.

The downloader intentionally requests a bounded range at a time. The supplied
`Nth` implementation retains each response in `rxBuffer`, so this design is
safe for large firmware without requiring an undocumented RX-buffer consume
operation.

## Integration

Provide `TmsPlatformOps` wrappers for your SDK file functions, initialize one
`TmsUpgrade`, and call `tmsUpgradeProcess()` from the application loop.

```c
static TmsUpgrade g_upgrade;

static void upgradeFinished(TmsUpgrade *upgrade, TmsUpgradeResult result,
                            void *userData)
{
    (void)upgrade;
    (void)userData;
    LOG_D("upgrade result: %d", result);
}

void startUpgrade(const TmsPlatformOps *platform)
{
    tmsUpgradeInit(&g_upgrade, "sabztms.ir", 18094, platform,
                   upgradeFinished, NULL);
    tmsUpgradeStart(&g_upgrade, serialNo, BRAND_CODE, appVersion, 1);
}

void applicationTick(void)
{
    tmsUpgradeProcess(&g_upgrade);
}
```

`UpgradeInfo` is taken from your existing `upgrade.h`. Its required fields are:
`responseCode`, `isHasNewVersion`, `message`, `planId`, `version`, `url`,
`fileSize`, `hash`, and `ts`.

