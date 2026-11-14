#include "wifiPwdMgr.h"
#include "storage/storage.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "logger.h"

/*
 * Stored layout V2. Binary BSSID uses 6 bytes instead of 18 printable bytes.
 * Do not change the persisted struct without a new version/migration.
 * C99 ABI: 116 bytes/record, 948-byte image for 8 slots on common ARM ABIs.
 */
typedef struct {
    char     ssid[WIFI_PM_SSID_CAPACITY];
    char     password[WIFI_PM_PASSWORD_CAPACITY];
    uint8_t  lastBssid[6]; /* all zero = not known */
    uint32_t security;
    uint32_t lastSuccess; /* monotonic success sequence (not time) */
    uint32_t successCount;
} WifiPmRecord;

typedef struct {
    uint32_t     magic;
    uint16_t     version;
    uint16_t     imageBytes;
    uint32_t     sequence;
    uint8_t      count;
    uint8_t      reserved[3];
    WifiPmRecord records[WIFI_PM_STORAGE_SLOTS];
    uint32_t     crc32;
} WifiPmImage;

/* Reserve >=16 bytes for storage file header + TLV framing. */
typedef char WifiPmMustFitInStorage[(sizeof(WifiPmImage) <= 1008u) ? 1 : -1];
typedef char WifiPmRecordSizeCheck[(sizeof(WifiPmRecord) == 116u) ? 1 : -1];

#define WIFI_PM_MAGIC       UINT32_C(0x57494650)
#define WIFI_PM_VERSION     2u
#define WIFI_PM_BSSID_BYTES 6u

static struct {
    WifiPmImage image;
    uint8_t     limit;
    bool        ready;
    bool        dirty;
} g_pm;

static WifiPwdMgr __wifiPwdMgr;

/* Storage layer keeps the referenced address for this call only. */
static const DataDescriptor g_wifiDsc[] = {
    DSC_BIN_ID(WIFI_PM_STORAGE_ID, g_pm.image)};

void wifiPmSecureZero(void* buffer, size_t bytes) {
    volatile uint8_t* p = (volatile uint8_t*)buffer;
    if (p == NULL)
        return;
    while (bytes-- != 0u)
        *p++ = 0u;
}

/* Returns maxLen+1 on invalid/unterminated input. */
static size_t boundedLen(const char* str, size_t maxLen) {
    size_t i;
    if (str == NULL)
        return maxLen + 1u;
    for (i = 0u; i <= maxLen; ++i)
        if (str[i] == '\0')
            return i;
    return maxLen + 1u;
}

static bool copyText(char* dst, size_t capacity, const char* src) {
    size_t len;
    if (dst == NULL || capacity == 0u)
        return false;
    len = boundedLen(src, capacity - 1u);
    if (len >= capacity)
        return false;
    memset(dst, 0, capacity);
    if (len != 0u)
        memcpy(dst, src, len);
    return true;
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* Strict colon-delimited BSSID parsing; malformed values are treated as
 * unknown. */
static bool parseBssid(const char* text, uint8_t out[6]) {
    unsigned i;
    if (text == NULL || boundedLen(text, 17u) != 17u)
        return false;
    for (i = 0u; i < 6u; ++i) {
        int hi = hexNibble(text[i * 3u]);
        int lo = hexNibble(text[i * 3u + 1u]);
        if (hi < 0 || lo < 0)
            return false;
        if (i < 5u && text[i * 3u + 2u] != ':')
            return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static bool knownBssid(const uint8_t mac[6]) {
    unsigned i;
    for (i = 0u; i < 6u; ++i)
        if (mac[i] != 0u)
            return true;
    return false;
}

/* Does not put passwords or SSIDs in logs. */
static uint32_t crc32Image(const void* data, size_t length) {
    const uint8_t* p   = (const uint8_t*)data;
    uint32_t       crc = UINT32_C(0xFFFFFFFF);
    size_t         i;
    for (i = 0u; i < length; ++i) {
        unsigned b;
        crc ^= p[i];
        for (b = 0u; b < 8u; ++b) {
            uint32_t mask = 0u - (crc & 1u);
            crc           = (crc >> 1u) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

static void sealImage(void) {
    g_pm.image.crc32 = crc32Image(&g_pm.image, offsetof(WifiPmImage, crc32));
}

static void emptyImage(void) {
    wifiPmSecureZero(&g_pm.image, sizeof(g_pm.image));
    g_pm.image.magic      = WIFI_PM_MAGIC;
    g_pm.image.version    = WIFI_PM_VERSION;
    g_pm.image.imageBytes = (uint16_t)sizeof(g_pm.image);
    sealImage();
}

static bool isAllZero(const void* data, size_t length) {
    const uint8_t* p = (const uint8_t*)data;
    size_t         i;
    for (i = 0u; i < length; ++i)
        if (p[i] != 0u)
            return false;
    return true;
}

static bool recordValid(const WifiPmRecord* rec) {
    size_t ssid = boundedLen(rec->ssid, 32u);
    size_t pass = boundedLen(rec->password, 64u);
    return ssid >= 1u && ssid <= 32u && pass <= 64u;
}

static bool imageValid(void) {
    uint8_t      i, j;
    WifiPmImage* image = &g_pm.image;
    if (image->magic != WIFI_PM_MAGIC || image->version != WIFI_PM_VERSION ||
        image->imageBytes != sizeof(*image) ||
        image->count > WIFI_PM_STORAGE_SLOTS ||
        image->crc32 != crc32Image(image, offsetof(WifiPmImage, crc32)))
        return false;
    for (i = 0u; i < image->count; ++i) {
        if (!recordValid(&image->records[i]))
            return false;
        for (j = (uint8_t)(i + 1u); j < image->count; ++j)
            if (image->records[i].security == image->records[j].security &&
                strcmp(image->records[i].ssid, image->records[j].ssid) == 0)
                return false;
    }
    return true;
}

static int findIndex(const char* ssid, uint32_t security) {
    uint8_t i;
    for (i = 0u; i < g_pm.image.count; ++i) {
        const WifiPmRecord* r = &g_pm.image.records[i];
        if (r->security == security && strcmp(r->ssid, ssid) == 0)
            return (int)i;
    }
    return -1;
}

static uint8_t oldestIndex(void) {
    uint8_t i, oldest = 0u;
    for (i = 1u; i < g_pm.image.count; ++i)
        if (g_pm.image.records[i].lastSuccess <
            g_pm.image.records[oldest].lastSuccess)
            oldest = i;
    return oldest;
}

static void eraseIndex(uint8_t index) {
    uint8_t i;
    uint8_t count = g_pm.image.count;
    if (index >= count)
        return;
    for (i = index; (unsigned)i + 1u < count; ++i)
        g_pm.image.records[i] = g_pm.image.records[i + 1u];
    wifiPmSecureZero(&g_pm.image.records[count - 1u], sizeof(WifiPmRecord));
    g_pm.image.count = (uint8_t)(count - 1u);
}

static WifiPmResult flush(void) {
    Error_t result;
    if (!g_pm.ready)
        return WIFI_PM_ERR_NOT_READY;
    sealImage();
    result = storage()->save(g_wifiDsc, 1u, WIFI_PM_STORAGE_PATH);
    if (result != ERR_OK) {
        g_pm.dirty = true;
        LOG_ERROR("Wi-Fi PM save failed: err=%d", (int)result);
        return WIFI_PM_ERR_IO;
    }
    g_pm.dirty = false;
    LOG_TRACE("Wi-Fi PM persisted %u networks", (unsigned)g_pm.image.count);
    return WIFI_PM_OK;
}

static WifiPmResult init(uint8_t limit) {
    Error_t result;
    if (limit == 0u || limit > WIFI_PM_STORAGE_SLOTS)
        return WIFI_PM_ERR_ARGUMENT;

    /* Initialize in a known state; unsuccessful reinit never exposes old data.
     */
    wifiPmSecureZero(&g_pm, sizeof(g_pm));
    g_pm.limit = limit;

    result = storage()->load(g_wifiDsc, 1u, WIFI_PM_STORAGE_PATH);
    if (result != ERR_OK) {
        LOG_ERROR("Wi-Fi PM load failed: err=%d", (int)result);
        wifiPmSecureZero(&g_pm.image, sizeof(g_pm.image));
        return WIFI_PM_ERR_IO;
    }

    /* storage()->load() saves a zero-filled default for an absent file. */
    if (isAllZero(&g_pm.image, sizeof(g_pm.image))) {
        emptyImage();
        g_pm.ready = true;
        result     = storage()->save(g_wifiDsc, 1u, WIFI_PM_STORAGE_PATH);
        if (result != ERR_OK) {
            g_pm.ready = false;
            LOG_ERROR("Wi-Fi PM first-time initialization failed: %d",
                      (int)result);
            return WIFI_PM_ERR_IO;
        }
        return WIFI_PM_OK;
    }

    if (!imageValid()) {
        /* If the outer Storage CRC was valid but inner image is corrupt,
         * preserve that blob. Unfortunately Storage may already have reset
         * an outer-CRC failure before we get here; see integration guide. */
        wifiPmSecureZero(&g_pm.image, sizeof(g_pm.image));
        LOG_ERROR("Wi-Fi PM image has incompatible version or invalid CRC");
        return WIFI_PM_ERR_CORRUPT;
    }

    g_pm.ready = true;
    if (g_pm.image.count > g_pm.limit) {
        while (g_pm.image.count > g_pm.limit)
            eraseIndex(oldestIndex());
        return flush();
    }
    return WIFI_PM_OK;
}

static WifiPmResult rememberSuccess(const WifiPmAccessPoint* ap,
                                    const char*              password) {
    WifiPmRecord* rec;
    uint32_t      previous = 0u;
    uint8_t       bssid[6] = {0};
    size_t        ssidLen, passLen;
    int           index;

    if (!g_pm.ready)
        return WIFI_PM_ERR_NOT_READY;
    if (ap == NULL || password == NULL)
        return WIFI_PM_ERR_ARGUMENT;
    ssidLen = boundedLen(ap->ssid, 32u);
    passLen = boundedLen(password, 64u);
    if (ssidLen == 0u || ssidLen > 32u || passLen > 64u)
        return WIFI_PM_ERR_ARGUMENT;
    if (ap->bssid != NULL)
        (void)parseBssid(ap->bssid, bssid);

    index = findIndex(ap->ssid, ap->security);
    if (index < 0) {
        if (g_pm.image.count < g_pm.limit)
            index = (int)g_pm.image.count++;
        else
            index = (int)oldestIndex();
    } else {
        previous = g_pm.image.records[index].successCount;
    }
    rec = &g_pm.image.records[index];
    wifiPmSecureZero(rec, sizeof(*rec));
    (void)copyText(rec->ssid, sizeof(rec->ssid), ap->ssid);
    (void)copyText(rec->password, sizeof(rec->password), password);
    memcpy(rec->lastBssid, bssid, sizeof(bssid));
    rec->security = ap->security;

    if (g_pm.image.sequence == UINT32_MAX) {
        /* Rebase rare wraparound, preserving relative order. */
        uint8_t i;
        for (i = 0u; i < g_pm.image.count; ++i)
            g_pm.image.records[i].lastSuccess = (uint32_t)(i + 1u);
        g_pm.image.sequence = g_pm.image.count;
    }
    rec->lastSuccess  = ++g_pm.image.sequence;
    rec->successCount = previous == UINT32_MAX ? UINT32_MAX : previous + 1u;
    return flush();
}

static WifiPmResult lookup(const WifiPmAccessPoint* ap, char* password,
                           size_t capacity) {
    int    index;
    size_t len;
    if (!g_pm.ready)
        return WIFI_PM_ERR_NOT_READY;
    if (ap == NULL || password == NULL || capacity == 0u ||
        boundedLen(ap->ssid, 32u) == 0u || boundedLen(ap->ssid, 32u) > 32u)
        return WIFI_PM_ERR_ARGUMENT;
    index = findIndex(ap->ssid, ap->security);
    if (index < 0)
        return WIFI_PM_ERR_NOT_FOUND;
    len = strlen(g_pm.image.records[index].password);
    if (len + 1u > capacity)
        return WIFI_PM_ERR_ARGUMENT;
    memcpy(password, g_pm.image.records[index].password, len + 1u);
    return WIFI_PM_OK;
}

static WifiPmResult forget(const char* ssid, uint32_t security) {
    int index;
    if (!g_pm.ready)
        return WIFI_PM_ERR_NOT_READY;
    if (boundedLen(ssid, 32u) == 0u || boundedLen(ssid, 32u) > 32u)
        return WIFI_PM_ERR_ARGUMENT;
    index = findIndex(ssid, security);
    if (index < 0)
        return WIFI_PM_ERR_NOT_FOUND;
    eraseIndex((uint8_t)index);
    return flush();
}

static WifiPmResult clear(void) {
    if (!g_pm.ready)
        return WIFI_PM_ERR_NOT_READY;
    emptyImage();
    return flush();
}

static WifiPmResult setLimit(uint8_t limit) {
    bool trimmed = false;
    if (!g_pm.ready)
        return WIFI_PM_ERR_NOT_READY;
    if (limit == 0u || limit > WIFI_PM_STORAGE_SLOTS)
        return WIFI_PM_ERR_ARGUMENT;
    g_pm.limit = limit;
    while (g_pm.image.count > limit) {
        eraseIndex(oldestIndex());
        trimmed = true;
    }
    return trimmed ? flush() : WIFI_PM_OK;
}

static uint8_t count(void) { return g_pm.ready ? g_pm.image.count : 0u; }

static bool isReady(void) { return g_pm.ready; }
static bool isDirty(void) { return g_pm.dirty; }

static bool triedAt(const uint8_t* tried, uint16_t index) {
    return (tried[index / 8u] & (uint8_t)(1u << (index % 8u))) != 0u;
}

static bool pickCandidate(WifiPmScanGetFn scanGet, void* scanCtx,
                          uint16_t apCount, const uint8_t* tried,
                          size_t triedBytes, WifiPmCandidate* out) {
    const WifiPmRecord* best                            = NULL;
    uint32_t            bestPriority                    = 0u;
    bool                bestBssid                       = false;
    uint16_t            bestIndex                       = 0u, i;
    char                bestMac[WIFI_PM_BSSID_CAPACITY] = {0};

    if (!g_pm.ready || scanGet == NULL || out == NULL || tried == NULL ||
        apCount > WIFI_PM_MAX_SCAN_APS ||
        triedBytes < ((size_t)apCount + 7u) / 8u)
        return false;

    for (i = 0u; i < apCount; ++i) {
        WifiPmAccessPoint   ap;
        int                 index;
        bool                matchingMac = false;
        uint8_t             mac[6]      = {0};
        const WifiPmRecord* rec;

        if (triedAt(tried, i))
            continue;
        memset(&ap, 0, sizeof(ap));
        if (!scanGet(scanCtx, i, &ap) || boundedLen(ap.ssid, 32u) == 0u ||
            boundedLen(ap.ssid, 32u) > 32u)
            continue;
        index = findIndex(ap.ssid, ap.security);
        if (index < 0)
            continue;
        rec = &g_pm.image.records[index];
        if (parseBssid(ap.bssid, mac) && knownBssid(rec->lastBssid))
            matchingMac = memcmp(mac, rec->lastBssid, 6u) == 0;

        if (best == NULL || rec->lastSuccess > bestPriority ||
            (rec->lastSuccess == bestPriority && matchingMac && !bestBssid)) {
            best         = rec;
            bestPriority = rec->lastSuccess;
            bestBssid    = matchingMac;
            bestIndex    = i;
            if (ap.bssid == NULL ||
                !copyText(bestMac, sizeof(bestMac), ap.bssid))
                bestMac[0] = '\0';
        }
    }

    if (best == NULL)
        return false;
    memset(out, 0, sizeof(*out));
    out->apIndex = bestIndex;
    memcpy(out->password, best->password, sizeof(out->password));
    memcpy(out->ssid, best->ssid, sizeof(out->ssid));
    memcpy(out->bssid, bestMac, sizeof(out->bssid));
    out->security = best->security;
    return true;
}

OOP_CTOR(WifiPwdMgr) {
    if (self == NULL)
        return;
    self->init            = init;
    self->rememberSuccess = rememberSuccess;
    self->lookup          = lookup;
    self->forget          = forget;
    self->clear           = clear;
    self->flush           = flush;
    self->setLimit        = setLimit;
    self->count           = count;
    self->isReady         = isReady;
    self->isDirty         = isDirty;
    self->pickCandidate   = pickCandidate;
}

WifiPwdMgr* wifiPwdMgr(void) {
    CALL_ONCE(OOP_CALL_CTOR(WifiPwdMgr, &__wifiPwdMgr););
    return &__wifiPwdMgr;
}
