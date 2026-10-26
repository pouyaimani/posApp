#include "emv_kahroba.h"
#include "emv_picc_transport.h"
#include "common.h"
#include "emv_terminal_hooks.h"
#include "utility/convert.h"

static const uint8_t KAHROBA_AID[] = {0xA0, 0x00, 0x00, 0x08, 0x03, 0x00, 0x01};

static EmvPiccAdapter adapter;

/**
 * Convert a decimal amount string to 6-byte packed BCD.
 *
 * Example:
 *   "123456"      -> 00 00 00 12 34 56
 *   "123456789012" -> 12 34 56 78 90 12
 *
 * No truncation, sign, separators or decimal point allowed.
 */
static bool encodeAmountBcd(const char* amount, uint8_t output[6]) {
    size_t len = 0u;

    if (!amount || !output)
        return false;

    memset(output, 0, 6u);

    while (len < 12u && amount[len] != '\0') {
        if (amount[len] < '0' || amount[len] > '9')
            return false;

        ++len;
    }

    if (len == 0u || amount[len] != '\0')
        return false;

    for (size_t i = 0u; i < len; ++i) {
        uint8_t digit = (uint8_t)(amount[len - 1u - i] - '0');

        size_t byteIndex = 5u - (i / 2u);

        if ((i & 1u) == 0u)
            output[byteIndex] |= digit;
        else
            output[byteIndex] |= (uint8_t)(digit << 4u);
    }

    return true;
}

static bool kahrobaDolValue(void* context, uint32_t tag, uint8_t* output,
                            size_t requiredLength) {
    const KahrobaGpoData* data = (const KahrobaGpoData*)context;

    if (!data || !output)
        return false;

    switch (tag) {

    /* Kahroba TTPI — proprietary */
    case 0xC7:
        if (requiredLength != 5u)
            return false;

        memcpy(output, data->ttpi, 5u);
        return true;

    /* Amount, Authorized */
    case 0x9F02:
        if (requiredLength != 6u)
            return false;

        return encodeAmountBcd(data->amountDigits, output);

    /* Transaction Currency Code */
    case 0x5F2A:
        if (requiredLength != 2u)
            return false;

        memcpy(output, data->currency, 2u);
        return true;

    /* Terminal Country Code */
    case 0x9F1A:
        if (requiredLength != 2u)
            return false;

        memcpy(output, data->country, 2u);
        return true;

    /* Terminal Type */
    case 0x9F35:
        if (requiredLength != 1u)
            return false;

        output[0] = data->terminalType;
        return true;

    /* Transaction Type */
    case 0x9C:
        if (requiredLength != 1u)
            return false;

        output[0] = data->transactionType;
        return true;

    default:
        LOG_TRACE("Kahroba: unsupported PDOL tag=%lX len=%lu",
                  (unsigned long)tag, (unsigned long)requiredLength);
        return false;
    }
}

static bool kahrobaRecordHandler(void* context, uint8_t sfi,
                                 uint8_t recordNumber, const uint8_t* data,
                                 size_t length) {
    KahrobaCardData* card = (KahrobaCardData*)context;
    EmvTlv           item;
    size_t           offset = 0u;

    if (!card || (!data && length != 0u))
        return false;

    LOG_TRACE("Kahroba: processing SFI=%u record=%u len=%lu", (unsigned int)sfi,
              (unsigned int)recordNumber, (unsigned long)length);

    while (offset < length) {

        if (!emvTlvNext(data, length, &offset, &item))
            return false;

        LOG_TRACE("Kahroba: tag=%lX length=%lu", (unsigned long)item.tag,
                  (unsigned long)item.length);

        switch (item.tag) {

        case 0x8C: /* CDOL1 */
            if (card->cdol1Len != 0u || item.length == 0u ||
                item.length > sizeof(card->cdol1))
                return false;

            memcpy(card->cdol1, item.value, item.length);
            card->cdol1Len = item.length;
            break;

        case 0x5A: /* PAN */
            if (card->panLen != 0u || item.length == 0u ||
                item.length > sizeof(card->pan))
                return false;

            memcpy(card->pan, item.value, item.length);
            card->panLen = item.length;
            break;

        case 0x57: /* Track 2 */
            if (card->track2Len != 0u || item.length == 0u ||
                item.length > sizeof(card->track2))
                return false;

            memcpy(card->track2, item.value, item.length);
            card->track2Len = item.length;
            break;

        case 0xDF70:
            if (card->hasDf70 || item.length != 1u)
                return false;

            card->df70    = item.value[0];
            card->hasDf70 = true;
            break;

        default:
            break;
        }
    }

    return true;
}

static bool kahrobaCdolValue(void* context, uint32_t tag, uint8_t* output,
                             size_t length) {
    const KahrobaAcData* data = (const KahrobaAcData*)context;

    if (!data || !output)
        return false;

    switch (tag) {

    case 0x9F02:
        if (length != 6u)
            return false;

        return encodeAmountBcd(data->amountDigits, output);

    case 0x9F03:
        if (length != 6u)
            return false;

        memset(output, 0, 6u);
        return true;

    case 0x9F1A:
        if (length != 2u)
            return false;

        memcpy(output, data->country, 2u);
        return true;

    case 0x95:
        if (length != 5u)
            return false;

        memcpy(output, data->tvr, 5u);
        return true;

    case 0x5F2A:
        if (length != 2u)
            return false;

        memcpy(output, data->currency, 2u);
        return true;

    case 0x9A:
        if (length != 3u)
            return false;

        memcpy(output, data->date, 3u);
        return true;

    case 0x9C:
        if (length != 1u)
            return false;

        output[0] = data->transactionType;
        return true;

    case 0x9F37:
        if (length != 4u)
            return false;

        memcpy(output, data->unpredictableNumber, 4u);
        return true;

    case 0x9F35:
        if (length != 1u)
            return false;

        output[0] = data->terminalType;
        return true;

    case 0x9F34:
        if (length != 3u)
            return false;

        memcpy(output, data->cvmResult, 3u);
        return true;

    case 0x9F21:
        if (length != 3u)
            return false;

        memcpy(output, data->time, 3u);
        return true;

    case 0x9F4E: {
        size_t copyLength;

        if (data->merchantNameLen != 0u && data->merchantName == NULL)
            return false;

        memset(output, 0x20, length);

        copyLength = data->merchantNameLen;

        if (copyLength > length)
            copyLength = length;

        if (copyLength != 0u) {
            memcpy(output, data->merchantName, copyLength);
        }

        return true;
    }

    default:
        LOG_TRACE("Kahroba: unsupported CDOL1 tag=%lX", (unsigned long)tag);
        return false;
    }
}

EmvMgrError startKahrobaEmv(Picc* reader, TxnData* txn) {
    EmvMgrError err;

    if (reader == NULL)
        return EMV_MGR_ERR_ARGUMENT;

    adapter.device = reader;

    emvMgr()->reset();

    err = emvMgr()->init(emvPiccTransport(&adapter));

    if (err != EMV_MGR_OK)
        return err;

    /* After successful detection of a compatible card. */
    err = OOP_CALL(picc(), activate);

    if (err != PICC_ERR_OK) {
        LOG_TRACE("Kahroba: PICC activation failed error=%d", (int)err);
        /* Release the retained reader and report failure. */
        return err;
    }

    emvMgr()->selectPpse(KAHROBA_AID, sizeof(KAHROBA_AID));

    if (err != EMV_MGR_OK) {
        LOG_TRACE("Kahroba: SELECT PPSE failed error=%d", (int)err);
        return err;
    }

    err = emvMgr()->selectAid(KAHROBA_AID, sizeof(KAHROBA_AID));

    LOG_TRACE("Kahroba: SELECT AID result=%d state=%d", (int)err,
              (int)emvMgr()->getState());

    if (err != EMV_MGR_OK)
        return err;

    uint64_t amount = txn->core.amount;

    DEFINE_STRING(amntStr, LEN_MAX_AMOUNT + 1);
    U64_TO_STRING(&amount, amntStr);

    KahrobaGpoData gpoData = {
        .amountDigits = amntStr,

        /* Preserve the legacy TTPI for initial compatibility. */
        .ttpi = {0x34, 0x00, 0x40, 0x02, 0x88},

        /*
         * Legacy currency: 0978 (EUR).
         * Must be confirmed against Kahroba specifications
         * before production deployment.
         */
        .currency = {0x09, 0x78},

        .country         = {0x03, 0x64},
        .terminalType    = 0x21,
        .transactionType = 0x00};

    err = emvMgr()->getProcessingOptions(kahrobaDolValue, &gpoData);

    LOG_TRACE("Kahroba: GPO result=%d state=%d", (int)err,
              (int)emvMgr()->getState());

    if (err != EMV_MGR_OK)
        return err;

    /* Phase 4: READ RECORD */
    KahrobaCardData cardData = {0};

    err = emvMgr()->readRecords(kahrobaRecordHandler, &cardData);

    size_t offset          = 0u;
    size_t totalDataLength = 0u;
    size_t tagCount        = 0u;

    while (offset < cardData.cdol1Len) {
        uint32_t tag = 0u;
        uint8_t  first;
        uint8_t  next;
        uint8_t  requiredLength;
        size_t   tagBytes = 1u;

        /* Read the first tag byte. */
        first = cardData.cdol1[offset++];

        if (first == 0x00u || first == 0xFFu) {
            LOG_TRACE("Kahroba: CDOL1 invalid tag");
            return false;
        }

        tag = first;

        /* Parse multi-byte BER tags. */
        if ((first & 0x1Fu) == 0x1Fu) {
            do {
                if (offset >= cardData.cdol1Len || tagBytes >= 3u) {
                    LOG_TRACE("Kahroba: CDOL1 invalid tag encoding");
                    return false;
                }

                next = cardData.cdol1[offset++];
                tag  = (tag << 8u) | next;
                ++tagBytes;

            } while ((next & 0x80u) != 0u);
        }

        /* Every DOL tag must be followed by one length byte. */
        if (offset >= cardData.cdol1Len) {
            LOG_TRACE("Kahroba: CDOL1 missing length");
            return false;
        }

        requiredLength = cardData.cdol1[offset++];

        LOG_TRACE("Kahroba: CDOL1 tag=%lX requiredLength=%u",
                  (unsigned long)tag, (unsigned int)requiredLength);

        totalDataLength += requiredLength;
        ++tagCount;

        /* Short APDU data limit. */
        if (totalDataLength > 255u) {
            LOG_TRACE("Kahroba: CDOL1 data exceeds APDU limit");
            return false;
        }
    }

    LOG_TRACE("Kahroba: CDOL1 parsed tags=%lu dataLength=%lu",
              (unsigned long)tagCount, (unsigned long)totalDataLength);

    if (err != EMV_MGR_OK)
        return err;

    if (cardData.cdol1Len == 0u) {
        LOG_TRACE("Kahroba: CDOL1 missing");
        return EMV_MGR_ERR_PROTOCOL;
    }

    KahrobaAcData       acData   = {0};
    EmvGenerateAcResult acResult = {0};

    char merchantName[64] = {0};

    /* Transaction parameters already validated for GPO. */
    acData.amountDigits = amntStr;

    memcpy(acData.country, gpoData.country, sizeof(acData.country));

    memcpy(acData.currency, gpoData.currency, sizeof(acData.currency));

    acData.terminalType    = gpoData.terminalType;
    acData.transactionType = gpoData.transactionType;

    /* RTC date and time */
    if (!getDateTimeBcd(acData.date, acData.time)) {
        LOG_TRACE("Kahroba: invalid terminal date/time");
        return EMV_MGR_ERR_PROTOCOL;
    }

    /* Merchant name */
    if (!emvGetMerchantName(merchantName, sizeof(merchantName))) {

        LOG_TRACE("Kahroba: merchant name unavailable");
        return EMV_MGR_ERR_PROTOCOL;
    }

    acData.merchantName    = (const uint8_t*)merchantName;
    acData.merchantNameLen = strlen(merchantName);

    /* Actual TVR and CVM processing results */
    if (!txnEmvGetVerifiedChecks(txn, /* Actual TxnMgr pointer, not NULL */
                                 acData.tvr, acData.cvmResult)) {

        LOG_TRACE("Kahroba: terminal checks unavailable");
        return EMV_MGR_ERR_PROTOCOL;
    }

    /*
     *   TODO
     * Generate a fresh unpredictable number using the
     * device's approved cryptographic random generator.
     * Do not reuse the legacy fixed F0 54 BB 66 value.
     */

    if (!txnEmvGetUnpredictableNumber(acData.unpredictableNumber)) {
        LOG_TRACE("Kahroba: unpredictable number unavailable");
        return EMV_MGR_ERR_PROTOCOL;
    }

    // /* Fresh unpredictable number */
    // if (!txnEmvGetUnpredictableNumber(acData.unpredictableNumber)) {

    //     LOG_TRACE("Kahroba: random generation failed");
    //     return EMV_MGR_ERR_PROTOCOL;
    // }

    /* Execute GENERATE AC only after all inputs are valid. */
    err = emvMgr()->generateAc(cardData.cdol1, cardData.cdol1Len,
                               kahrobaCdolValue, &acData, &acResult);

    if (err != EMV_MGR_OK)
        return err;

    switch (acResult.cid & 0xC0u) {

    case 0x80u:
        LOG_TRACE("Kahroba: ARQC received - online required");
        /* Continue to online authorization. */
        break;

    case 0x00u:
        LOG_TRACE("Kahroba: AAC received - card declined");
        /* Stop transaction as declined. */
        break;

    case 0x40u:
        LOG_TRACE("Kahroba: TC received");
        /* Handle according to the applicable scheme rules. */
        break;

    default:
        LOG_TRACE("Kahroba: invalid cryptogram type");
        /* Reject unexpected response. */
        break;
    }

    if (err != EMV_MGR_OK)
        return err;

    return EMV_MGR_OK;
}