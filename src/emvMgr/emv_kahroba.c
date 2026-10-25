#include "emv_kahroba.h"
#include "emv_picc_transport.h"
#include "common.h"
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
    return err;
}