#include "emvMgr.h"

#include <stdbool.h>
#include <string.h>

#include "logger.h"

#define EMV_PPSE_RESPONSE_MAX 512u

typedef struct {
    /* Existing fields — keep unchanged */
    bool         initialized;
    EmvMgrState  state;
    EmvTransport transport;

    uint8_t response[512];
    size_t  responseLen;
    uint8_t sw1;
    uint8_t sw2;

    /* GPO results */
    uint8_t aip[2];   /* Application Interchange Profile */
    uint8_t afl[252]; /* Application File Locator */
    size_t  aflLen;   /* Actual AFL length in bytes */

} EmvContext;

static EmvContext ctx;

static const uint8_t PPSE_NAME[] = {'2', 'P', 'A', 'Y', '.', 'S', 'Y',
                                    'S', '.', 'D', 'D', 'F', '0', '1'};

static void secureWipe(void* buffer, size_t length) {
    volatile uint8_t* p = (volatile uint8_t*)buffer;

    while (length-- != 0u)
        *p++ = 0u;
}

/**
 * Find PDOL (9F38) inside:
 *
 * 6F
 *   84 ...
 *   A5
 *     9F38 ...
 *
 * PDOL is optional. If absent, length is zero.
 */
static bool extractPdol(const uint8_t* fci, size_t fciLength,
                        const uint8_t** pdol, size_t* pdolLength) {
    EmvTlv root;
    EmvTlv child;
    EmvTlv item;

    size_t offset    = 0u;
    bool   foundA5   = false;
    bool   foundPdol = false;

    if (!fci || !pdol || !pdolLength)
        return false;

    *pdol       = NULL;
    *pdolLength = 0u;

    if (!emvTlvNext(fci, fciLength, &offset, &root))
        return false;

    if (root.tag != 0x6Fu || offset != fciLength)
        return false;

    offset = 0u;

    while (offset < root.length) {
        if (!emvTlvNext(root.value, root.length, &offset, &child))
            return false;

        if (child.tag != 0xA5u)
            continue;

        if (foundA5)
            return false;

        foundA5 = true;

        size_t innerOffset = 0u;

        while (innerOffset < child.length) {
            if (!emvTlvNext(child.value, child.length, &innerOffset, &item))
                return false;

            if (item.tag == 0x9F38u) {
                if (foundPdol)
                    return false;

                *pdol       = item.value;
                *pdolLength = item.length;
                foundPdol   = true;
            }
        }
    }

    return foundA5;
}

static bool parseGpoResponse(const uint8_t* response, size_t responseLength) {
    EmvTlv root;
    EmvTlv item;

    const uint8_t* aip = NULL;
    const uint8_t* afl = NULL;

    size_t aipLen = 0u;
    size_t aflLen = 0u;
    size_t offset = 0u;

    bool foundAip = false;
    bool foundAfl = false;

    if (!response || responseLength < 2u)
        return false;

    if (!emvTlvNext(response, responseLength, &offset, &root))
        return false;

    if (offset != responseLength)
        return false;

    if (root.tag == 0x80u) {

        /* Format 1: AIP (2 bytes) followed by AFL. */
        if (root.length < 2u)
            return false;

        aip    = root.value;
        aipLen = 2u;

        afl    = root.value + 2u;
        aflLen = root.length - 2u;

    } else if (root.tag == 0x77u) {

        /* Format 2: constructed response. */
        offset = 0u;

        while (offset < root.length) {
            if (!emvTlvNext(root.value, root.length, &offset, &item))
                return false;

            if (item.tag == 0x82u) {
                if (foundAip)
                    return false;

                aip      = item.value;
                aipLen   = item.length;
                foundAip = true;

            } else if (item.tag == 0x94u) {
                if (foundAfl)
                    return false;

                afl      = item.value;
                aflLen   = item.length;
                foundAfl = true;
            }
        }

        if (!foundAip || !foundAfl)
            return false;

    } else {
        return false;
    }

    if (!aip || !afl || aipLen != 2u || aflLen > sizeof(ctx.afl) ||
        (aflLen % 4u) != 0u)
        return false;

    /* Validate AFL entries before using them for READ RECORD. */
    for (size_t i = 0u; i < aflLen; i += 4u) {
        uint8_t sfi       = (uint8_t)(afl[i] >> 3u);
        uint8_t first     = afl[i + 1u];
        uint8_t last      = afl[i + 2u];
        uint8_t authCount = afl[i + 3u];

        if ((afl[i] & 0x07u) != 0u || sfi == 0u || sfi > 30u || first == 0u ||
            last < first || authCount > (unsigned int)(last - first + 1u))
            return false;
    }

    memcpy(ctx.aip, aip, 2u);

    if (aflLen != 0u)
        memcpy(ctx.afl, afl, aflLen);

    ctx.aflLen = aflLen;

    LOG_TRACE("EMV: GPO parsed AIP=%02X%02X AFL entries=%lu", ctx.aip[0],
              ctx.aip[1], (unsigned long)(ctx.aflLen / 4u));

    return true;
}

/**
 * @brief Search a BER-TLV structure for a specific AID (tag 0x4F).
 *
 * Recursively processes constructed TLVs and compares primitive
 * tag 0x4F values against the requested AID.
 *
 * @param data     BER-TLV encoded data.
 * @param length   Length of data in bytes.
 * @param aid      AID to search for.
 * @param aidLen   Length of AID.
 * @param depth    Current recursion depth (initially 0).
 * @param found    Output flag, set to true if matching AID exists.
 *
 * @return true  TLV structure is valid.
 * @return false TLV structure is malformed or nesting is too deep.
 *
 * @note A valid TLV without a matching AID returns true with
 *       *found == false.
 */
static bool findAid(const uint8_t* data, size_t length, const uint8_t* aid,
                    size_t aidLen, unsigned depth, bool* found) {
    size_t pos = 0u;

    if (data == NULL || aid == NULL || found == NULL || depth > 8u) {
        LOG_TRACE("EMV TLV: invalid arguments or depth=%u", depth);
        return false;
    }

    while (pos < length) {
        uint32_t tag;
        uint8_t  first;
        bool     constructed;
        size_t   valueLen;
        size_t   count;
        size_t   i;

        /* Read tag identifier. */
        first       = data[pos++];
        constructed = (first & 0x20u) != 0u;
        tag         = first;

        /* Extended BER tag identifier. */
        if ((first & 0x1Fu) == 0x1Fu) {
            unsigned tagBytes = 0u;
            uint8_t  b;

            do {
                if (pos >= length || ++tagBytes > 3u) {
                    LOG_TRACE("EMV TLV: invalid extended tag");
                    return false;
                }

                b   = data[pos++];
                tag = (tag << 8u) | b;

            } while ((b & 0x80u) != 0u);
        }

        /* Read value length. */
        if (pos >= length) {
            LOG_TRACE("EMV TLV: missing length");
            return false;
        }

        first = data[pos++];

        if ((first & 0x80u) == 0u) {
            valueLen = first;
        } else {
            count = first & 0x7Fu;

            /* Only definite lengths up to two length bytes. */
            if (count == 0u || count > 2u || count > length - pos) {
                LOG_TRACE("EMV TLV: invalid length encoding");
                return false;
            }

            valueLen = 0u;

            for (i = 0u; i < count; ++i) {
                valueLen = (valueLen << 8u) | data[pos++];
            }

            /* Reject nonminimal BER length encoding. */
            if (valueLen < 128u || (count == 2u && valueLen <= 255u)) {
                LOG_TRACE("EMV TLV: nonminimal length encoding");
                return false;
            }
        }

        /* Prevent reading beyond the response buffer. */
        if (valueLen > length - pos) {
            LOG_TRACE("EMV TLV: value exceeds buffer");
            return false;
        }

        /* Compare application identifier (tag 4F). */
        if (tag == 0x4Fu && !constructed && valueLen == aidLen &&
            memcmp(data + pos, aid, aidLen) == 0) {

            *found = true;

            LOG_TRACE("EMV TLV: matching AID found");
        }

        /* Recursively inspect constructed TLV values. */
        if (constructed) {
            if (!findAid(data + pos, valueLen, aid, aidLen, depth + 1u,
                         found)) {
                return false;
            }
        }

        pos += valueLen;
    }

    return true;
}

/** Clear session data without releasing the transport. */
static void reset(void) {
    secureWipe(ctx.response, sizeof(ctx.response));

    ctx.responseLen = 0u;
    ctx.sw1         = 0u;
    ctx.sw2         = 0u;

    ctx.state = EMV_MGR_IDLE;

    memset(ctx.aip, 0, sizeof(ctx.aip));
    memset(ctx.afl, 0, sizeof(ctx.afl));
    ctx.aflLen = 0u;
}

/**
 * @brief Initialize EMV manager for a new activated PICC session.
 *
 * Resets previous session data and binds the supplied transport.
 *
 * @param transport Initialized EMV transport interface.
 * @return EMV_MGR_OK on success.
 */
static EmvMgrError init(EmvTransport transport) {

    if (transport.exchange == NULL) {
        LOG_TRACE("EMV: init rejected invalid transport");
        return EMV_MGR_ERR_ARGUMENT;
    }

    /*
     * Reject initialization while an EMV session is active.
     * Completed and failed sessions can be restarted.
     */
    if (ctx.initialized && ctx.state != EMV_MGR_IDLE &&
        ctx.state != EMV_MGR_FAILED && ctx.state != EMV_MGR_GPO_COMPLETED) {

        LOG_TRACE("EMV: init rejected active state=%d", (int)ctx.state);

        return EMV_MGR_ERR_STATE;
    }

    /* Clear previous EMV session data. */
    reset();

    /* Bind transport for the new session. */
    ctx.transport   = transport;
    ctx.initialized = true;
    ctx.state       = EMV_MGR_IDLE;

    LOG_TRACE("EMV: manager initialized");

    return EMV_MGR_OK;
}

static EmvMgrState getState(void) { return ctx.state; }

static EmvMgrError selectPpse(const uint8_t* aid, size_t aidLen) {
    EmvApduCommand  cmd  = {0};
    EmvApduResponse resp = {0};

    EmvMgrError result;
    bool        found = false;
    int         rc;

    if (!ctx.initialized || ctx.transport.exchange == NULL)
        return EMV_MGR_ERR_STATE;

    if (aid == NULL || aidLen < 5u || aidLen > 16u)
        return EMV_MGR_ERR_ARGUMENT;

    if (ctx.state != EMV_MGR_IDLE)
        return EMV_MGR_ERR_STATE;

    cmd.cla      = 0x00u;
    cmd.ins      = 0xA4u;
    cmd.p1       = 0x04u;
    cmd.p2       = 0x00u;
    cmd.data     = PPSE_NAME;
    cmd.data_len = sizeof(PPSE_NAME);
    /*
     * Match the working legacy SELECT PPSE command.
     *
     * The Trendit SDK previously used mLe=0 for this APDU.
     */
    cmd.has_le = false;
    cmd.le     = 0u;

    resp.data     = ctx.response;
    resp.capacity = sizeof(ctx.response);

    LOG_TRACE("EMV: SELECT PPSE begin");

    rc = ctx.transport.exchange(ctx.transport.context, &cmd, &resp);

    if (rc == -2) {
        result = EMV_MGR_ERR_TIMEOUT;
        goto failed;
    }

    if (rc != 0) {
        result = EMV_MGR_ERR_TRANSPORT;
        goto failed;
    }

    if (resp.length > resp.capacity) {
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    ctx.responseLen = resp.length;
    ctx.sw1         = resp.sw1;
    ctx.sw2         = resp.sw2;

    LOG_TRACE("EMV: PPSE response len=%lu SW=%02X%02X",
              (unsigned long)resp.length, (unsigned)resp.sw1,
              (unsigned)resp.sw2);

    if (resp.sw1 != 0x90u || resp.sw2 != 0x00u) {
        result = EMV_MGR_ERR_CARD_STATUS;
        goto failed;
    }

    if (resp.length < 2u || resp.data[0] != 0x6Fu) {
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    if (!findAid(resp.data, resp.length, aid, aidLen, 0u, &found)) {
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    if (!found) {
        result = EMV_MGR_ERR_UNSUPPORTED;
        goto failed;
    }

    ctx.state = EMV_MGR_PPSE_SELECTED;

    LOG_TRACE("EMV: Kahroba AID found in PPSE");

    return EMV_MGR_OK;

failed:
    LOG_TRACE("EMV: SELECT PPSE failed error=%d", (int)result);

    secureWipe(ctx.response, sizeof(ctx.response));
    ctx.responseLen = 0u;
    ctx.state       = EMV_MGR_FAILED;

    return result;
}

/**
 * @brief Decode a definite BER-TLV length.
 *
 * Supports short-form and long-form lengths up to two bytes.
 */
static bool readTlvLength(const uint8_t* data, size_t size, size_t* pos,
                          size_t* valueLen) {
    uint8_t first;
    size_t  count;
    size_t  i;
    size_t  value = 0u;

    if (data == NULL || pos == NULL || valueLen == NULL || *pos >= size)
        return false;

    first = data[(*pos)++];

    if ((first & 0x80u) == 0u) {
        *valueLen = first;
        return true;
    }

    count = first & 0x7Fu;

    if (count == 0u || count > 2u || count > size - *pos)
        return false;

    for (i = 0u; i < count; ++i) {
        value = (value << 8u) | data[(*pos)++];
    }

    if (value < 128u || (count == 2u && value <= 255u))
        return false;

    *valueLen = value;
    return true;
}

/**
 * @brief Validate SELECT AID FCI and its DF Name.
 *
 * Expected structure:
 *
 * 6F
 *   84 <selected AID>
 *   A5 ...
 *
 * Tag 84 must be a direct child of 6F.
 */
static bool findDfName(const uint8_t* data, size_t length, const uint8_t* aid,
                       size_t aidLen) {
    size_t pos = 1u;
    size_t fciLen;
    size_t fciEnd;
    bool   found = false;

    if (data == NULL || aid == NULL || length < 2u || data[0] != 0x6Fu)
        return false;

    if (!readTlvLength(data, length, &pos, &fciLen))
        return false;

    if (fciLen != length - pos)
        return false;

    fciEnd = length;

    while (pos < fciEnd) {
        uint8_t tag;
        size_t  valueLen;

        tag = data[pos++];

        /*
         * Direct children of FCI typically include
         * primitive tag 84 and constructed tag A5.
         *
         * This Phase 2 validator supports single-byte
         * tags at this level.
         */
        if ((tag & 0x1Fu) == 0x1Fu)
            return false;

        if (!readTlvLength(data, fciEnd, &pos, &valueLen))
            return false;

        if (valueLen > fciEnd - pos)
            return false;

        if (tag == 0x84u) {
            if (found || valueLen != aidLen ||
                memcmp(data + pos, aid, aidLen) != 0)
                return false;

            found = true;
        }

        pos += valueLen;
    }

    return found;
}

/**
 * @brief Select an EMV application using its AID.
 *
 * Requirements:
 * - SELECT PPSE must have succeeded.
 * - The PICC reader must remain activated.
 * - The transport must be initialized.
 *
 * On success, the SELECT AID response is retained in ctx.response
 * for subsequent FCI/PDOL processing.
 *
 * @param aid    Application identifier to select.
 * @param aidLen AID length in bytes (5..16).
 *
 * @return EMV_MGR_OK on success, otherwise an EMV manager error.
 */
static EmvMgrError selectAid(const uint8_t* aid, size_t aidLen) {
    EmvApduCommand  cmd  = {0};
    EmvApduResponse resp = {0};

    EmvMgrError result;
    int         rc;

    if (!ctx.initialized || ctx.transport.exchange == NULL)
        return EMV_MGR_ERR_STATE;

    if (aid == NULL || aidLen < 5u || aidLen > 16u)
        return EMV_MGR_ERR_ARGUMENT;

    if (ctx.state != EMV_MGR_PPSE_SELECTED) {
        LOG_TRACE("EMV: SELECT AID rejected state=%d", (int)ctx.state);
        return EMV_MGR_ERR_STATE;
    }

    /*
     * SELECT by DF name:
     * CLA = 00
     * INS = A4
     * P1  = 04
     * P2  = 00
     * Data = AID
     *
     * Le=0 matches the working Trendit legacy SDK behavior.
     */
    cmd.cla = 0x00u;
    cmd.ins = 0xA4u;
    cmd.p1  = 0x04u;
    cmd.p2  = 0x00u;

    cmd.data     = aid;
    cmd.data_len = aidLen;

    cmd.has_le = false;
    cmd.le     = 0u;

    /*
     * Clear the previous PPSE response before storing
     * the SELECT AID response.
     */
    secureWipe(ctx.response, sizeof(ctx.response));
    ctx.responseLen = 0u;
    ctx.sw1         = 0u;
    ctx.sw2         = 0u;

    resp.data     = ctx.response;
    resp.capacity = sizeof(ctx.response);

    LOG_TRACE("EMV: SELECT AID begin aidLen=%lu", (unsigned long)aidLen);

    rc = ctx.transport.exchange(ctx.transport.context, &cmd, &resp);

    if (rc == -2) {
        result = EMV_MGR_ERR_TIMEOUT;
        goto failed;
    }

    if (rc != 0) {
        result = EMV_MGR_ERR_TRANSPORT;
        goto failed;
    }

    if (resp.length > resp.capacity) {
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    ctx.responseLen = resp.length;
    ctx.sw1         = resp.sw1;
    ctx.sw2         = resp.sw2;

    LOG_TRACE("EMV: SELECT AID response len=%lu SW=%02X%02X",
              (unsigned long)resp.length, (unsigned int)resp.sw1,
              (unsigned int)resp.sw2);

    if (resp.sw1 != 0x90u || resp.sw2 != 0x00u) {
        result = EMV_MGR_ERR_CARD_STATUS;
        goto failed;
    }

    /*
     * FCI must begin with the 6F template.
     * Further validation follows below.
     */
    if (resp.length < 2u || resp.data[0] != 0x6Fu) {
        LOG_TRACE("EMV: SELECT AID invalid FCI template");
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    /*
     * SELECT AID FCI should contain tag 84 (DF Name)
     * matching the selected AID.
     */
    if (!findDfName(resp.data, resp.length, aid, aidLen)) {
        LOG_TRACE("EMV: SELECT AID missing/mismatched DF name");
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    ctx.state = EMV_MGR_AID_SELECTED;

    LOG_TRACE("EMV: SELECT AID completed successfully");

    LOG_TRACE("EMV: SELECT AID FCI length=%lu", (unsigned long)ctx.responseLen);

    for (size_t i = 0; i < ctx.responseLen; i += 16u) {
        char   line[16 * 3 + 1];
        size_t pos = 0u;

        for (size_t j = i; j < ctx.responseLen && j - i < 16u; ++j) {
            int n = snprintf(line + pos, sizeof(line) - pos, "%02X ",
                             ctx.response[j]);

            if (n < 0 || (size_t)n >= sizeof(line) - pos)
                break;

            pos += (size_t)n;
        }

        line[pos] = '\0';
        LOG_TRACE("EMV FCI: %s", line);
    }

    return EMV_MGR_OK;

failed:
    LOG_TRACE("EMV: SELECT AID failed error=%d", (int)result);

    secureWipe(ctx.response, sizeof(ctx.response));
    ctx.responseLen = 0u;
    ctx.state       = EMV_MGR_FAILED;

    return result;
}

static EmvMgrError getProcessingOptions(EmvDolValueFn provider,
                                        void*         providerContext) {
    const uint8_t* pdol         = NULL;
    size_t         pdolLength   = 0u;
    size_t         valuesLength = 0u;

    uint8_t dolValues[127]   = {0};
    uint8_t commandData[129] = {0};

    EmvApduCommand  cmd  = {0};
    EmvApduResponse resp = {0};

    EmvMgrError result;
    int         rc;

    if (!ctx.initialized || ctx.transport.exchange == NULL ||
        ctx.state != EMV_MGR_AID_SELECTED)
        return EMV_MGR_ERR_STATE;

    if (!provider)
        return EMV_MGR_ERR_ARGUMENT;

    LOG_TRACE("EMV: GPO begin");

    /*
     * SELECT AID FCI is still stored in ctx.response.
     */
    if (!extractPdol(ctx.response, ctx.responseLen, &pdol, &pdolLength)) {

        LOG_TRACE("EMV: GPO failed parsing FCI");
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    LOG_TRACE("EMV: GPO PDOL definition length=%lu", (unsigned long)pdolLength);

    if (!emvDolBuild(pdol, pdolLength, provider, providerContext, dolValues,
                     sizeof(dolValues), &valuesLength)) {

        LOG_TRACE("EMV: GPO failed building PDOL");
        result = EMV_MGR_ERR_ARGUMENT;
        goto failed;
    }

    /*
     * This implementation supports short-form 83 lengths
     * up to 127 bytes. The current card requests 17 bytes.
     *
     * Data = 83 <length> <concatenated DOL values>
     */
    commandData[0] = 0x83u;
    commandData[1] = (uint8_t)valuesLength;

    if (valuesLength != 0u) {
        memcpy(commandData + 2u, dolValues, valuesLength);
    }

    cmd.cla = 0x80u;
    cmd.ins = 0xA8u;
    cmd.p1  = 0x00u;
    cmd.p2  = 0x00u;

    cmd.data     = commandData;
    cmd.data_len = valuesLength + 2u;

    /*
     * Keep the confirmed working Trendit SDK convention.
     */
    cmd.has_le = false;
    cmd.le     = 0u;

    /* The FCI is no longer needed after building GPO. */
    secureWipe(ctx.response, sizeof(ctx.response));
    ctx.responseLen = 0u;

    memset(ctx.aip, 0, sizeof(ctx.aip));
    memset(ctx.afl, 0, sizeof(ctx.afl));
    ctx.aflLen = 0u;

    resp.data     = ctx.response;
    resp.capacity = sizeof(ctx.response);

    LOG_TRACE("EMV: GPO exchange Lc=%lu PDOL bytes=%lu",
              (unsigned long)cmd.data_len, (unsigned long)valuesLength);

    rc = ctx.transport.exchange(ctx.transport.context, &cmd, &resp);

    if (rc == -2) {
        result = EMV_MGR_ERR_TIMEOUT;
        goto failed;
    }

    if (rc != 0) {
        result = EMV_MGR_ERR_TRANSPORT;
        goto failed;
    }

    if (resp.length > resp.capacity) {
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    ctx.responseLen = resp.length;

    LOG_TRACE("EMV: GPO response len=%lu SW=%02X%02X",
              (unsigned long)resp.length, resp.sw1, resp.sw2);

    if (resp.sw1 != 0x90u || resp.sw2 != 0x00u) {
        result = EMV_MGR_ERR_CARD_STATUS;
        goto failed;
    }

    if (!parseGpoResponse(ctx.response, ctx.responseLen)) {

        LOG_TRACE("EMV: GPO invalid AIP/AFL");
        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    for (size_t i = 0; i < ctx.aflLen; i += 4u) {
        uint8_t sfi       = (uint8_t)(ctx.afl[i] >> 3u);
        uint8_t first     = ctx.afl[i + 1u];
        uint8_t last      = ctx.afl[i + 2u];
        uint8_t authCount = ctx.afl[i + 3u];

        LOG_TRACE("EMV AFL[%lu]: SFI=%u first=%u last=%u authCount=%u",
                  (unsigned long)(i / 4u), (unsigned int)sfi,
                  (unsigned int)first, (unsigned int)last,
                  (unsigned int)authCount);
    }

    ctx.state = EMV_MGR_GPO_COMPLETED;

    LOG_TRACE("EMV: GPO completed successfully");

    secureWipe(dolValues, sizeof(dolValues));
    secureWipe(commandData, sizeof(commandData));

    return EMV_MGR_OK;

failed:
    LOG_TRACE("EMV: GPO failed error=%d", (int)result);

    secureWipe(dolValues, sizeof(dolValues));
    secureWipe(commandData, sizeof(commandData));
    secureWipe(ctx.response, sizeof(ctx.response));

    ctx.responseLen = 0u;
    memset(ctx.aip, 0, sizeof(ctx.aip));
    memset(ctx.afl, 0, sizeof(ctx.afl));
    ctx.aflLen = 0u;
    ctx.state  = EMV_MGR_FAILED;

    return result;
}

#define EMV_MAX_RECORD_EXCHANGES 64u

/**
 * @brief Read all application records specified by the AFL.
 *
 * Requires a successful GPO operation.
 *
 * On success:
 *   ctx.state = EMV_MGR_RECORDS_READ
 *
 * On failure:
 *   ctx.state = EMV_MGR_FAILED
 *
 * @param callback Record-processing callback.
 * @param userData User context passed to callback.
 *
 * @return EMV_MGR_OK on success.
 */
static EmvMgrError readRecords(EmvRecordFn callback, void* userData) {
    EmvApduCommand  cmd;
    EmvApduResponse resp;
    EmvTlv          record;

    EmvMgrError result = EMV_MGR_OK;

    size_t totalRecords = 0u;
    size_t offset;

    int rc;

    if (!ctx.initialized || ctx.transport.exchange == NULL ||
        ctx.state != EMV_MGR_GPO_COMPLETED) {

        LOG_TRACE("EMV: READ RECORD rejected state=%d", (int)ctx.state);

        return EMV_MGR_ERR_STATE;
    }

    if (callback == NULL)
        return EMV_MGR_ERR_ARGUMENT;

    if (ctx.aflLen == 0u || ctx.aflLen > sizeof(ctx.afl) ||
        (ctx.aflLen % 4u) != 0u) {

        result = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    /*
     * Validate the entire AFL before sending APDUs.
     */
    for (size_t i = 0u; i < ctx.aflLen; i += 4u) {
        uint8_t sfi       = (uint8_t)(ctx.afl[i] >> 3u);
        uint8_t first     = ctx.afl[i + 1u];
        uint8_t last      = ctx.afl[i + 2u];
        uint8_t authCount = ctx.afl[i + 3u];

        if ((ctx.afl[i] & 0x07u) != 0u || sfi == 0u || sfi > 30u ||
            first == 0u || last < first ||
            authCount > (unsigned int)(last - first + 1u)) {

            result = EMV_MGR_ERR_PROTOCOL;
            goto failed;
        }

        totalRecords += (size_t)last - first + 1u;

        if (totalRecords > EMV_MAX_RECORD_EXCHANGES) {
            LOG_TRACE("EMV: AFL exceeds record limit");
            result = EMV_MGR_ERR_PROTOCOL;
            goto failed;
        }
    }

    LOG_TRACE("EMV: READ RECORD begin entries=%lu records=%lu",
              (unsigned long)(ctx.aflLen / 4u), (unsigned long)totalRecords);

    for (size_t i = 0u; i < ctx.aflLen; i += 4u) {

        uint8_t sfi = (uint8_t)(ctx.afl[i] >> 3u);

        uint8_t first = ctx.afl[i + 1u];

        uint8_t last = ctx.afl[i + 2u];

        for (unsigned int number = first; number <= last; ++number) {

            memset(&cmd, 0, sizeof(cmd));
            memset(&resp, 0, sizeof(resp));

            cmd.cla = 0x00u;
            cmd.ins = 0xB2u;
            cmd.p1  = (uint8_t)number;
            cmd.p2  = (uint8_t)((sfi << 3u) | 0x04u);

            cmd.data     = NULL;
            cmd.data_len = 0u;

            /* Trendit-compatible Le convention. */
            cmd.has_le = false;
            cmd.le     = 0u;

            /*
             * Reuse the response buffer.
             * The callback must copy any data it needs later.
             */
            secureWipe(ctx.response, sizeof(ctx.response));

            ctx.responseLen = 0u;

            resp.data     = ctx.response;
            resp.capacity = sizeof(ctx.response);

            LOG_TRACE("EMV: READ RECORD SFI=%u record=%u P2=%02X",
                      (unsigned int)sfi, number, (unsigned int)cmd.p2);

            rc = ctx.transport.exchange(ctx.transport.context, &cmd, &resp);

            if (rc == -2) {
                result = EMV_MGR_ERR_TIMEOUT;
                goto failed;
            }

            if (rc != 0) {
                result = EMV_MGR_ERR_TRANSPORT;
                goto failed;
            }

            if (resp.length > resp.capacity) {
                result = EMV_MGR_ERR_PROTOCOL;
                goto failed;
            }

            ctx.responseLen = resp.length;

            LOG_TRACE("EMV: READ RECORD response SFI=%u "
                      "record=%u len=%lu SW=%02X%02X",
                      (unsigned int)sfi, number, (unsigned long)resp.length,
                      (unsigned int)resp.sw1, (unsigned int)resp.sw2);

            if (resp.sw1 != 0x90u || resp.sw2 != 0x00u) {

                result = EMV_MGR_ERR_CARD_STATUS;
                goto failed;
            }

            /*
             * EMV READ RECORD normally returns:
             *
             * 70 <length> <record TLVs>
             */
            offset = 0u;

            if (!emvTlvNext(resp.data, resp.length, &offset, &record)) {

                result = EMV_MGR_ERR_PROTOCOL;
                goto failed;
            }

            if (record.tag != 0x70u || offset != resp.length) {

                LOG_TRACE("EMV: READ RECORD invalid template");

                result = EMV_MGR_ERR_PROTOCOL;
                goto failed;
            }

            /*
             * Parse/retain record data through the callback.
             */
            if (!callback(userData, sfi, (uint8_t)number, record.value,
                          record.length)) {

                LOG_TRACE("EMV: READ RECORD handler rejected record");

                result = EMV_MGR_ERR_PROTOCOL;
                goto failed;
            }
        }
    }

    ctx.state = EMV_MGR_RECORDS_READ;

    LOG_TRACE("EMV: READ RECORD completed successfully");

    return EMV_MGR_OK;

failed:
    LOG_TRACE("EMV: READ RECORD failed error=%d", (int)result);

    secureWipe(ctx.response, sizeof(ctx.response));

    ctx.responseLen = 0u;
    ctx.state       = EMV_MGR_FAILED;

    return result;
}

static bool parseGenerateAcResponse(const uint8_t* data, size_t length,
                                    EmvGenerateAcResult* out) {
    EmvTlv root;
    EmvTlv item;
    size_t offset = 0u;

    bool hasCid = false;
    bool hasAtc = false;
    bool hasAc  = false;
    bool hasIad = false;

    if (!data || !out)
        return false;

    memset(out, 0, sizeof(*out));

    if (!emvTlvNext(data, length, &offset, &root) || offset != length)
        return false;

    if (root.tag == 0x80u) {
        /*
         * Format 1:
         * CID(1) + ATC(2) + AC(8) + optional IAD.
         */
        if (root.length < 11u || root.length - 11u > sizeof(out->iad))
            return false;

        out->cid = root.value[0];

        memcpy(out->atc, root.value + 1u, 2u);
        memcpy(out->ac, root.value + 3u, 8u);

        out->iadLen = root.length - 11u;

        if (out->iadLen != 0u) {
            memcpy(out->iad, root.value + 11u, out->iadLen);
        }

        return true;
    }

    if (root.tag != 0x77u)
        return false;

    /*
     * Format 2: constructed BER-TLV.
     */
    offset = 0u;

    while (offset < root.length) {
        if (!emvTlvNext(root.value, root.length, &offset, &item))
            return false;

        switch (item.tag) {

        case 0x9F27: /* CID */
            if (hasCid || item.length != 1u)
                return false;

            out->cid = item.value[0];
            hasCid   = true;
            break;

        case 0x9F36: /* ATC */
            if (hasAtc || item.length != 2u)
                return false;

            memcpy(out->atc, item.value, 2u);
            hasAtc = true;
            break;

        case 0x9F26: /* AC */
            if (hasAc || item.length != 8u)
                return false;

            memcpy(out->ac, item.value, 8u);
            hasAc = true;
            break;

        case 0x9F10: /* IAD */
            if (hasIad || item.length > sizeof(out->iad))
                return false;

            if (item.length != 0u)
                memcpy(out->iad, item.value, item.length);

            out->iadLen = item.length;
            hasIad      = true;
            break;

        default:
            /* Ignore unrelated optional TLVs. */
            break;
        }
    }

    return hasCid && hasAtc && hasAc;
}

static EmvMgrError generateAc(const uint8_t* cdol1, size_t cdol1Len,
                              EmvDolValueFn provider, void* providerContext,
                              EmvGenerateAcResult* result) {
    uint8_t commandData[255] = {0};
    size_t  commandLength    = 0u;

    EmvApduCommand  cmd  = {0};
    EmvApduResponse resp = {0};

    EmvMgrError error = EMV_MGR_OK;
    int         rc;

    if (!ctx.initialized || ctx.transport.exchange == NULL ||
        ctx.state != EMV_MGR_RECORDS_READ)
        return EMV_MGR_ERR_STATE;

    if (!cdol1 || cdol1Len == 0u || !provider || !result)
        return EMV_MGR_ERR_ARGUMENT;

    memset(result, 0, sizeof(*result));

    LOG_TRACE("EMV: GENERATE AC begin");

    if (!emvDolBuild(cdol1, cdol1Len, provider, providerContext, commandData,
                     sizeof(commandData), &commandLength)) {

        LOG_TRACE("EMV: GENERATE AC CDOL1 build failed");

        error = EMV_MGR_ERR_ARGUMENT;
        goto failed;
    }

    if (commandLength == 0u) {
        error = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    LOG_TRACE("EMV: GENERATE AC CDOL1 data length=%lu",
              (unsigned long)commandLength);

    LOG_TRACE("EMV: GENERATE AC CDOL1 length=%lu",
              (unsigned long)commandLength);

    /*
     * Debug only: inspect terminal-supplied CDOL values.
     * Do not enable unrestricted payment data logging
     * in production.
     */
    for (size_t i = 0u; i < commandLength; i += 16u) {
        char   line[16 * 3 + 1] = {0};
        size_t pos              = 0u;

        for (size_t j = i; j < commandLength && j < i + 16u; ++j) {

            int n = snprintf(line + pos, sizeof(line) - pos, "%02X ",
                             commandData[j]);

            if (n < 0 || (size_t)n >= sizeof(line) - pos)
                break;

            pos += (size_t)n;
        }

        LOG_TRACE("EMV: CDOL1 offset=%lu data=%s", (unsigned long)i, line);
    }

    cmd.cla = 0x80u;
    cmd.ins = 0xAEu;
    cmd.p1  = 0x80u; /* Request ARQC, no CDA requested */
    cmd.p2  = 0x00u;

    cmd.data     = commandData;
    cmd.data_len = commandLength;

    /* Existing Trendit SDK convention */
    cmd.has_le = false;
    cmd.le     = 0u;

    secureWipe(ctx.response, sizeof(ctx.response));
    ctx.responseLen = 0u;

    resp.data     = ctx.response;
    resp.capacity = sizeof(ctx.response);

    rc = ctx.transport.exchange(ctx.transport.context, &cmd, &resp);

    if (rc == -2) {
        error = EMV_MGR_ERR_TIMEOUT;
        goto failed;
    }

    if (rc != 0) {
        error = EMV_MGR_ERR_TRANSPORT;
        goto failed;
    }

    if (resp.length > resp.capacity) {
        error = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    ctx.responseLen = resp.length;

    LOG_TRACE("EMV: GENERATE AC response len=%lu SW=%02X%02X",
              (unsigned long)resp.length, (unsigned int)resp.sw1,
              (unsigned int)resp.sw2);

    if (resp.sw1 != 0x90u || resp.sw2 != 0x00u) {
        error = EMV_MGR_ERR_CARD_STATUS;
        goto failed;
    }

    if (!parseGenerateAcResponse(resp.data, resp.length, result)) {

        LOG_TRACE("EMV: GENERATE AC invalid response");

        error = EMV_MGR_ERR_PROTOCOL;
        goto failed;
    }

    LOG_TRACE("EMV: GENERATE AC parsed CID=%02X ATC=%02X%02X IAD len=%lu",
              (unsigned int)result->cid, (unsigned int)result->atc[0],
              (unsigned int)result->atc[1], (unsigned long)result->iadLen);

    ctx.state = EMV_MGR_AC_GENERATED;

    secureWipe(commandData, sizeof(commandData));
    secureWipe(ctx.response, sizeof(ctx.response));
    ctx.responseLen = 0u;

    return EMV_MGR_OK;

failed:
    LOG_TRACE("EMV: GENERATE AC failed error=%d", (int)error);

    memset(result, 0, sizeof(*result));
    secureWipe(commandData, sizeof(commandData));
    secureWipe(ctx.response, sizeof(ctx.response));

    ctx.responseLen = 0u;
    ctx.state       = EMV_MGR_FAILED;

    return error;
}

EmvMgr* emvMgr(void) {
    static EmvMgr manager = {
        .init                 = init,
        .selectPpse           = selectPpse,
        .getState             = getState,
        .reset                = reset,
        .selectAid            = selectAid,
        .getProcessingOptions = getProcessingOptions,
        .readRecords          = readRecords,
        .generateAc           = generateAc,
    };

    return &manager;
}