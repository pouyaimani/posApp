#include "emvMgr.h"

#include <stdbool.h>
#include <string.h>

#include "logger.h"

#define EMV_PPSE_RESPONSE_MAX 512u

typedef struct {
    EmvTransport transport;
    EmvMgrState  state;

    uint8_t response[EMV_PPSE_RESPONSE_MAX];
    size_t  responseLen;

    uint8_t sw1;
    uint8_t sw2;

    bool initialized;
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
}

/** Initialize the manager for an activated PICC reader. */
static EmvMgrError init(EmvTransport transport) {
    if (transport.exchange == NULL)
        return EMV_MGR_ERR_ARGUMENT;

    /*
     * Do not rebind the transport during an active session.
     * For Phase 1, any non-idle state requires reset().
     */
    if (ctx.initialized && ctx.state != EMV_MGR_IDLE)
        return EMV_MGR_ERR_STATE;

    reset();

    ctx.transport   = transport;
    ctx.initialized = true;

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

EmvMgr* emvMgr(void) {
    static EmvMgr manager = {.init       = init,
                             .selectPpse = selectPpse,
                             .getState   = getState,
                             .reset      = reset};

    return &manager;
}