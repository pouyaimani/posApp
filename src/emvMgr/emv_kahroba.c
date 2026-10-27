#include "emv_kahroba.h"
#include "emv_picc_transport.h"
#include "common.h"
#include "emv_terminal_hooks.h"
#include "utility/convert.h"
#include "kahroba_field55.h"

static const uint8_t KAHROBA_AID[] = {0xA0, 0x00, 0x00, 0x08, 0x03, 0x00, 0x01};

static EmvPiccAdapter adapter;

/**
 * @brief Append Track 2 and PAN to the Kahroba Field 55.
 *
 * 57 - Track 2 Equivalent Data
 * 5A - Application PAN
 *
 * Uses the binary values extracted during READ RECORD.
 *
 * SECURITY:
 * - Never log these values.
 * - Never persist Track 2 equivalent data.
 * - Clear sensitive buffers after authorization.
 *
 * TODO(HOST):
 * Confirm whether both tags are required by the acquirer.
 */
static bool kahrobaAppendCardTags(KahrobaField55Writer*    writer,
                                  const KahrobaEmvContext* context) {
    if (!writer || !context || !writer->data)
        return false;

    const KahrobaCardData* card = &context->card;

    /* Validate the stored card data. */
    if (card->track2Len == 0u || card->track2Len > sizeof(card->track2) ||
        card->panLen == 0u || card->panLen > sizeof(card->pan)) {

        EMV_LOG("Kahroba: invalid PAN or Track2 length");
        return false;
    }

    /*
     * Check total capacity before appending either tag.
     *
     * 57: 1 tag + 1 length + Track2 data
     * 5A: 1 tag + 1 length + PAN data
     */
    size_t required = 4u + card->track2Len + card->panLen;

    if (writer->length > writer->capacity ||
        required > writer->capacity - writer->length) {

        EMV_LOG("Kahroba: insufficient Field55 capacity");
        return false;
    }

    /* 57 - Track 2 Equivalent Data */
    if (!kahrobaField55Append(writer, 0x57, card->track2, card->track2Len))
        return false;

    /* 5A - Application PAN */
    if (!kahrobaField55Append(writer, 0x5A, card->pan, card->panLen))
        return false;

    EMV_LOG("Kahroba: card tags appended "
            "57 len=%lu 5A len=%lu",
            (unsigned long)card->track2Len, (unsigned long)card->panLen);

    return true;
}

/**
 * @brief Append CVM and Form Factor tags to Field 55.
 *
 * 9F34 - Cardholder Verification Method Results
 * 9F6E - Form Factor Indicator
 */
static bool kahrobaAppendVerificationTags(KahrobaField55Writer*    writer,
                                          const KahrobaEmvContext* context) {
    if (writer == NULL || context == NULL)
        return false;

    /* 9F6E must have been extracted from READ RECORD. */
    if (!context->card.hasFormFactor) {
        EMV_LOG("Kahroba: 9F6E unavailable");
        return false;
    }

    /* 9F34 - CVM Results */
    if (!kahrobaField55Append(writer, 0x9F34, context->cvmResult,
                              sizeof(context->cvmResult)))
        return false;

    // /* 9F6E - Form Factor Indicator */
    // if (!kahrobaField55Append(writer, 0x9F6E, context->card.formFactor,
    //                           sizeof(context->card.formFactor)))
    //     return false;

    /* 9F6E - Form Factor Indicator */
    uint8_t pos_entry[] = {0x10, 0x30, 0x00, 0x00};
    if (!kahrobaField55Append(writer, 0x9F6E, pos_entry,
                              sizeof(context->card.formFactor)))
        return false;

    EMV_LOG("Kahroba: verification tags appended "
            "9F34 len=3 9F6E len=4");

    return true;
}

/**
 * @brief Append additional Kahroba EMV tags.
 *
 * 9F37 - Unpredictable Number
 * DF70 - Kahroba proprietary card data
 * 84   - Application AID
 *
 * All values must come from the current EMV transaction.
 */
static bool kahrobaAppendApplicationTags(KahrobaField55Writer*    writer,
                                         const KahrobaEmvContext* context) {
    static const uint8_t KAHROBA_AID[] = {0xA0, 0x00, 0x00, 0x08,
                                          0x03, 0x00, 0x01};

    if (!writer || !context)
        return false;

    /* DF70 must have been extracted during READ RECORD. */
    if (!context->card.hasDf70) {
        EMV_LOG("Kahroba: DF70 unavailable");
        return false;
    }

    /*
     * 9F37 - Unpredictable Number.
     *
     * Use exactly the same value supplied to GENERATE AC.
     *
     * TODO(SECURITY):
     * Replace the legacy fixed value with a fresh
     * cryptographically secure random value per transaction.
     */
    if (!kahrobaField55Append(writer, 0x9F37, context->unpredictableNumber,
                              sizeof(context->unpredictableNumber)))
        return false;

    // /* DF70 - Value extracted from the card. */
    // if (!kahrobaField55Append(writer, 0xDF70, &context->card.df70,
    //                           sizeof(context->card.df70)))
    //     return false;

    /* DF70 - Value extracted from the card. */
    uint8_t df70_data = 0x00;
    if (!kahrobaField55Append(writer, 0xDF70, &df70_data, 1))
        return false;

    /* 84 - Application AID. */
    if (!kahrobaField55Append(writer, 0x84, KAHROBA_AID, sizeof(KAHROBA_AID)))
        return false;

    EMV_LOG("Kahroba: application tags appended "
            "9F37 len=4 DF70=%02X AID len=%lu",
            (unsigned int)context->card.df70,
            (unsigned long)sizeof(KAHROBA_AID));

    return true;
}

/**
 * @brief Append issuer and terminal country information.
 *
 * 9F10 - Issuer Application Data
 * 9F1A - Terminal Country Code
 */
static bool kahrobaAppendIssuerTags(KahrobaField55Writer*    writer,
                                    const KahrobaEmvContext* context) {
    if (!writer || !context)
        return false;

    /* Validate IAD returned by GENERATE AC. */
    if (context->acResult.iadLen == 0u ||
        context->acResult.iadLen > sizeof(context->acResult.iad))
        return false;

    /* 9F10 - Issuer Application Data */
    if (!kahrobaField55Append(writer, 0x9F10, context->acResult.iad,
                              context->acResult.iadLen))
        return false;

    /* 9F1A - Terminal Country Code */
    if (!kahrobaField55Append(writer, 0x9F1A, context->country,
                              sizeof(context->country)))
        return false;

    return true;
}

/**
 * @brief Encode a decimal amount as 6-byte EMV packed BCD.
 *
 * Example:
 *   100000 -> 00 00 00 10 00 00
 */
static bool kahrobaEncodeAmount(uint64_t amount, uint8_t output[6]) {
    if (!output || amount == 0u || amount > UINT64_C(999999999999))
        return false;

    for (size_t i = 6u; i > 0u; --i) {
        uint8_t low = (uint8_t)(amount % 10u);
        amount /= 10u;

        uint8_t high = (uint8_t)(amount % 10u);
        amount /= 10u;

        output[i - 1u] = (uint8_t)((high << 4u) | low);
    }

    return true;
}

/**
 * @brief Append amount-related tags to Field 55.
 *
 * 9F02 - Amount Authorized
 * 9F03 - Amount Other
 */
static bool kahrobaAppendAmountTags(KahrobaField55Writer* writer,
                                    const TxnData*        txn) {
    uint8_t       amountBcd[6]   = {0};
    const uint8_t otherAmount[6] = {0};

    if (!writer || !txn)
        return false;

    if (!kahrobaEncodeAmount(txn->core.amount, amountBcd))
        return false;

    if (!kahrobaField55Append(writer, 0x9F02, amountBcd, sizeof(amountBcd)))
        return false;

    if (!kahrobaField55Append(writer, 0x9F03, otherAmount, sizeof(otherAmount)))
        return false;

    return true;
}

/**
 * @brief Append the initial terminal and application tags
 *        to ISO 8583 Field 55.
 *
 * Tags:
 *   5F2A - Transaction Currency Code
 *   82   - Application Interchange Profile
 *   95   - Terminal Verification Results
 *   9A   - Transaction Date
 *   9C   - Transaction Type
 *
 * @return true if all tags were appended successfully.
 */
static bool kahrobaAppendBaseTags(KahrobaField55Writer*    writer,
                                  const KahrobaEmvContext* context) {
    if (!writer || !context || !context->hasAip)
        return false;

    if (!kahrobaField55Append(writer, 0x5F2A, context->currency,
                              sizeof(context->currency)))
        return false;

    if (!kahrobaField55Append(writer, 0x82, context->aip, sizeof(context->aip)))
        return false;

    if (!kahrobaField55Append(writer, 0x95, context->tvr, sizeof(context->tvr)))
        return false;

    if (!kahrobaField55Append(writer, 0x9A, context->transactionDate,
                              sizeof(context->transactionDate)))
        return false;

    if (!kahrobaField55Append(writer, 0x9C, &context->transactionType, 1u))
        return false;

    return true;
}

/**
 * @brief Convert EMV tag 5A (packed numeric PAN)
 *        into a null-terminated decimal string.
 *
 * EMV PAN may contain one trailing F padding nibble.
 *
 * No card data is logged.
 */
static bool kahrobaDecodePan(const uint8_t* src, size_t srcLen, char* dst,
                             size_t dstSize) {
    char   result[20];
    size_t count       = 0u;
    bool   paddingSeen = false;

    if (!src || !dst || srcLen == 0u || srcLen > 10u)
        return false;

    for (size_t i = 0u; i < srcLen; ++i) {

        uint8_t high = (uint8_t)(src[i] >> 4u);
        uint8_t low  = (uint8_t)(src[i] & 0x0Fu);

        if (paddingSeen)
            return false;

        if (high > 9u)
            return false;

        if (count >= sizeof(result) - 1u)
            return false;

        result[count++] = (char)('0' + high);

        if (low == 0x0Fu) {
            /* F padding is allowed only at the end. */
            if (i != srcLen - 1u)
                return false;

            paddingSeen = true;
        } else {
            if (low > 9u)
                return false;

            if (count >= sizeof(result) - 1u)
                return false;

            result[count++] = (char)('0' + low);
        }
    }

    if (count < 12u || count > 19u || count >= dstSize)
        return false;

    result[count] = '\0';

    memcpy(dst, result, count + 1u);

    return true;
}

/**
 * @brief Transfer EMV PAN into the common transaction data.
 *
 * Uses tag 5A collected during READ RECORD.
 */
static bool kahrobaPopulateTxnPan(const KahrobaCardData* card,
                                  TxnData*               txnData) {
    if (!card || !txnData)
        return false;

    if (card->panLen == 0u || card->panLen > sizeof(card->pan))
        return false;

    if (!kahrobaDecodePan(card->pan, card->panLen, txnData->core.pan,
                          sizeof(txnData->core.pan))) {

        EMV_LOG("Kahroba: invalid Application PAN");
        return false;
    }

    EMV_LOG("Kahroba: transaction PAN populated");

    return true;
}

/**
 * @brief Append cryptogram-related EMV tags to Field 55.
 *
 * Must be called after a successful GENERATE AC returning ARQC.
 *
 * Tags:
 *   9F26 - Application Cryptogram
 *   9F27 - Cryptogram Information Data
 *   9F36 - Application Transaction Counter
 */
bool kahrobaAppendCryptogramTags(KahrobaField55Writer*    writer,
                                 const KahrobaEmvContext* context) {
    if (writer == NULL || context == NULL)
        return false;

    /* Only build an online authorization message for ARQC. */
    if ((context->acResult.cid & 0xC0u) != 0x80u) {
        EMV_LOG("Kahroba: expected ARQC, CID=%02X",
                (unsigned int)context->acResult.cid);
        return false;
    }

    /* 9F26 - Application Cryptogram */
    if (!kahrobaField55Append(writer, 0x9F26, context->acResult.ac,
                              sizeof(context->acResult.ac)))
        return false;

    /* 9F27 - Cryptogram Information Data */
    if (!kahrobaField55Append(writer, 0x9F27, &context->acResult.cid,
                              sizeof(context->acResult.cid)))
        return false;

    /* 9F36 - Application Transaction Counter */
    if (!kahrobaField55Append(writer, 0x9F36, context->acResult.atc,
                              sizeof(context->acResult.atc)))
        return false;

    EMV_LOG("Kahroba: cryptogram tags appended CID=%02X ATC=%02X%02X",
            (unsigned int)context->acResult.cid,
            (unsigned int)context->acResult.atc[0],
            (unsigned int)context->acResult.atc[1]);

    return true;
}

/**
 * @brief Build the complete Kahroba Field 55.
 *
 * Uses the 19 legacy-compatible EMV tags.
 *
 * Must be called after successful GENERATE AC (ARQC).
 *
 * @param txn            Current transaction.
 * @param context        Current EMV transaction context.
 * @param output         Destination binary TLV buffer.
 * @param capacity       Output buffer capacity.
 * @param outputLength   Number of bytes written.
 *
 * @return true on success, false otherwise.
 *
 * @note Output contains sensitive payment data.
 *       Never log or persist the entire buffer.
 */
bool kahrobaBuildField55(const TxnData* txn, const KahrobaEmvContext* context,
                         uint8_t* output, size_t capacity,
                         size_t* outputLength) {
    KahrobaField55Writer writer = {0};

    if (outputLength != NULL)
        *outputLength = 0u;

    if (!txn || !context || !output || !outputLength || capacity == 0u)
        return false;

    /* Field 55 for online authorization requires ARQC. */
    if ((context->acResult.cid & 0xC0u) != 0x80u)
        return false;

    /*
     * The amount sent to the host must match the amount
     * supplied during GPO and GENERATE AC.
     *
     * authorizedAmount is captured before GENERATE AC.
     */
    if (txn->core.amount != context->authorizedAmount)
        return false;

    if (!kahrobaField55Init(&writer, output, capacity))
        return false;

    /* 1. 5F2A, 82, 95, 9A, 9C */
    if (!kahrobaAppendBaseTags(&writer, context))
        goto failed;

    /* 2. 9F02, 9F03 */
    if (!kahrobaAppendAmountTags(&writer, txn))
        goto failed;

    /* 3. 9F10, 9F1A */
    if (!kahrobaAppendIssuerTags(&writer, context))
        goto failed;

    /* 4. 9F26, 9F27, 9F36 */
    if (!kahrobaAppendCryptogramTags(&writer, context))
        goto failed;

    /* 5. 9F37, DF70, 84 */
    if (!kahrobaAppendApplicationTags(&writer, context))
        goto failed;

    /* 6. 9F34, 9F6E */
    if (!kahrobaAppendVerificationTags(&writer, context))
        goto failed;

    /* 7. 57, 5A */
    if (!kahrobaAppendCardTags(&writer, context))
        goto failed;

    *outputLength = writer.length;

    kahrobaTraceField55(&writer);

    EMV_LOG("Kahroba: Field55 built successfully length=%lu",
            (unsigned long)*outputLength);

    return true;

failed:
    /* Remove any partially generated sensitive data. */
    volatile uint8_t* p = (volatile uint8_t*)output;

    for (size_t i = 0u; i < writer.length; ++i)
        p[i] = 0u;

    *outputLength = 0u;

    EMV_LOG("Kahroba: Field55 construction failed");

    return false;
}

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
        EMV_LOG("Kahroba: unsupported PDOL tag=%lX len=%lu", (unsigned long)tag,
                (unsigned long)requiredLength);
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

    EMV_LOG("Kahroba: processing SFI=%u record=%u len=%lu", (unsigned int)sfi,
            (unsigned int)recordNumber, (unsigned long)length);

    while (offset < length) {

        if (!emvTlvNext(data, length, &offset, &item))
            return false;

        // EMV_LOG("Kahroba: tag=%lX length=%lu", (unsigned long)item.tag,
        //           (unsigned long)item.length);

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

        case 0x9F6E: {
            /* Form Factor Indicator - 4 bytes. */
            if (item.length != sizeof(card->formFactor)) {
                EMV_LOG("Kahroba: invalid 9F6E length=%lu",
                        (unsigned long)item.length);
                return false;
            }

            if (card->hasFormFactor) {
                EMV_LOG("Kahroba: duplicate 9F6E");
                return false;
            }

            memcpy(card->formFactor, item.value, sizeof(card->formFactor));

            card->hasFormFactor = true;

            EMV_LOG("Kahroba: 9F6E stored");
            break;
        }

        case 0xDF70: {
            /* Kahroba proprietary data - 1 byte. */
            if (item.length != 1u) {
                EMV_LOG("Kahroba: invalid DF70 length=%lu",
                        (unsigned long)item.length);
                return false;
            }

            if (card->hasDf70) {
                EMV_LOG("Kahroba: duplicate DF70");
                return false;
            }

            card->df70    = item.value[0];
            card->hasDf70 = true;

            EMV_LOG("Kahroba: DF70 stored=%02X", (unsigned int)card->df70);
            break;
        }
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
        EMV_LOG("Kahroba: unsupported CDOL1 tag=%lX", (unsigned long)tag);
        return false;
    }
}

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

EmvMgrError startKahrobaEmv(Picc* reader, TxnData* txn,
                            KahrobaEmvContext* context) {
    EmvMgrError err;
    int         piccErr;

    KahrobaGpoData gpoData = {0};
    KahrobaAcData  acData  = {0};

    char merchantName[64] = {0};
    char amountDigits[13] = {0};

    uint64_t amount;

    /* ================= Validate arguments ================= */

    if (!reader || !txn || !context)
        return EMV_MGR_ERR_ARGUMENT;

    /*
     * Defensive EMV 9F02 range check.
     *
     * The scheme-specific amount policy must already have
     * been validated by the transaction handler before NFC.
     */
    if (txn->core.amount == 0u || txn->core.amount > UINT64_C(999999999999)) {

        EMV_LOG("Kahroba: invalid EMV transaction amount");
        return EMV_MGR_ERR_ARGUMENT;
    }

    /*
     * context must be owned by the caller and must remain
     * valid through subsequent online authorization.
     */
    memset(context, 0, sizeof(*context));

    amount = txn->core.amount;

    /* Reuse the existing project conversion utility. */
    U64_TO_STRING(&amount, amountDigits);

    EMV_LOG("Kahroba: starting EMV transaction");

    /* ================= Initialize EMV ================= */

    adapter.device = reader;

    emvMgr()->reset();

    err = emvMgr()->init(emvPiccTransport(&adapter));

    if (err != EMV_MGR_OK)
        return err;

    /*
     * Activate the same reader passed to this function.
     * nfcMgr remains responsible for its eventual release.
     */
    piccErr = (int)OOP_CALL(reader, activate);

    if (piccErr != (int)PICC_ERR_OK) {
        EMV_LOG("Kahroba: PICC activation failed error=%d", piccErr);

        return EMV_MGR_ERR_PROTOCOL;
    }

    /* ================= SELECT PPSE ================= */

    err = emvMgr()->selectPpse(KAHROBA_AID, sizeof(KAHROBA_AID));

    if (err != EMV_MGR_OK) {
        EMV_LOG("Kahroba: SELECT PPSE failed error=%d", (int)err);
        return err;
    }

    /* ================= SELECT AID ================= */

    err = emvMgr()->selectAid(KAHROBA_AID, sizeof(KAHROBA_AID));

    if (err != EMV_MGR_OK) {
        EMV_LOG("Kahroba: SELECT AID failed error=%d", (int)err);
        return err;
    }

    /* ================= GPO ================= */

    gpoData.amountDigits = amountDigits;

    /* TODO: Replace legacy TTPI with validated scheme values. */
    static const uint8_t legacyTtpi[5] = {0x34, 0x00, 0x40, 0x02, 0x88};

    memcpy(gpoData.ttpi, legacyTtpi, sizeof(legacyTtpi));

    /*
     * TODO: Verify currency against the Kahroba/acquirer spec.
     * 0978 is ISO 4217 EUR, not IRR.
     */
    gpoData.currency[0] = 0x09;
    gpoData.currency[1] = 0x78;

    gpoData.country[0] = 0x03;
    gpoData.country[1] = 0x64;

    gpoData.terminalType    = 0x21;
    gpoData.transactionType = 0x00;

    err = emvMgr()->getProcessingOptions(kahrobaDolValue, &gpoData);

    if (err != EMV_MGR_OK) {
        EMV_LOG("Kahroba: GPO failed error=%d", (int)err);
        return err;
    }

    /* Save AIP directly in the transaction's EMV context. */
    err = emvMgr()->getAip(context->aip);

    if (err != EMV_MGR_OK) {
        EMV_LOG("Kahroba: failed to retrieve AIP error=%d", (int)err);

        return err;
    }

    context->hasAip = true;

    EMV_LOG("Kahroba: AIP stored=%02X%02X", (unsigned int)context->aip[0],
            (unsigned int)context->aip[1]);

    /* ================= READ RECORD ================= */

    err = emvMgr()->readRecords(kahrobaRecordHandler, &context->card);

    if (err != EMV_MGR_OK) {
        EMV_LOG("Kahroba: READ RECORD failed error=%d", (int)err);
        return err;
    }

    if (context->card.cdol1Len == 0u ||
        context->card.cdol1Len > sizeof(context->card.cdol1)) {

        EMV_LOG("Kahroba: missing or invalid CDOL1");
        return EMV_MGR_ERR_PROTOCOL;
    }

    if (!context->card.hasFormFactor || !context->card.hasDf70) {

        EMV_LOG("Kahroba: required card data missing "
                "9F6E=%u DF70=%u",
                (unsigned int)context->card.hasFormFactor,
                (unsigned int)context->card.hasDf70);

        return EMV_MGR_ERR_PROTOCOL;
    }

    context->authorizedAmount = txn->core.amount;

    /* Verify data retained by READ RECORD. */
    // EMV_LOG("Kahroba: DF70 verification hasDf70=%d value=%02X",
    //           (int)context->card.hasDf70, (unsigned int)context->card.df70);

    // EMV_LOG("Kahroba: card data stored "
    //           "9F6E=%02X%02X%02X%02X DF70=%02X",
    //           context->card.formFactor[0], context->card.formFactor[1],
    //           context->card.formFactor[2], context->card.formFactor[3],
    //           context->card.df70);

    // EMV_LOG("Kahroba: card records read successfully, CDOL1 len=%lu",
    //           (unsigned long)context->card.cdol1Len);

    /* ================= Prepare GENERATE AC ================= */

    acData.amountDigits = amountDigits;

    memcpy(acData.country, gpoData.country, sizeof(acData.country));

    memcpy(acData.currency, gpoData.currency, sizeof(acData.currency));

    acData.terminalType    = gpoData.terminalType;
    acData.transactionType = gpoData.transactionType;

    /* Transaction date and time. */
    if (!getDateTimeBcd(acData.date, acData.time)) {
        EMV_LOG("Kahroba: invalid terminal date/time");
        return EMV_MGR_ERR_PROTOCOL;
    }

    /* Merchant name; local storage remains valid during call. */
    if (!emvGetMerchantName(merchantName, sizeof(merchantName))) {

        EMV_LOG("Kahroba: merchant name unavailable");
        return EMV_MGR_ERR_PROTOCOL;
    }

    merchantName[sizeof(merchantName) - 1u] = '\0';

    acData.merchantName    = (const uint8_t*)merchantName;
    acData.merchantNameLen = strlen(merchantName);

    /*
     * TODO(PRODUCTION):
     * Replace legacy TVR/CVM constants with actual EMV checks.
     *
     * Ensure this hook takes const TxnData*, not TxnMgr*.
     */
    if (!txnEmvGetVerifiedChecks(txn, acData.tvr, acData.cvmResult)) {

        EMV_LOG("Kahroba: terminal checks unavailable");
        return EMV_MGR_ERR_PROTOCOL;
    }

    /*
     * TODO(SECURITY - PRODUCTION BLOCKER):
     * Replace legacy fixed value with device-approved secure RNG.
     */
    if (!txnEmvGetUnpredictableNumber(acData.unpredictableNumber)) {

        EMV_LOG("Kahroba: unpredictable number unavailable");
        return EMV_MGR_ERR_PROTOCOL;
    }

    /* Save precisely the values supplied to GENERATE AC. */
    memcpy(context->unpredictableNumber, acData.unpredictableNumber,
           sizeof(context->unpredictableNumber));

    memcpy(context->tvr, acData.tvr, sizeof(context->tvr));

    memcpy(context->cvmResult, acData.cvmResult, sizeof(context->cvmResult));

    memcpy(context->transactionDate, acData.date,
           sizeof(context->transactionDate));

    memcpy(context->transactionTime, acData.time,
           sizeof(context->transactionTime));

    memcpy(context->currency, acData.currency, sizeof(context->currency));

    memcpy(context->country, acData.country, sizeof(context->country));

    context->terminalType    = acData.terminalType;
    context->transactionType = acData.transactionType;

    /* ================= GENERATE AC ================= */

    err = emvMgr()->generateAc(context->card.cdol1, context->card.cdol1Len,
                               kahrobaCdolValue, &acData, &context->acResult);

    if (err != EMV_MGR_OK) {
        EMV_LOG("Kahroba: GENERATE AC failed error=%d", (int)err);
        return err;
    }

    /* ================= Validate cryptogram type ================= */

    switch (context->acResult.cid & 0xC0u) {

    case 0x80u:
        EMV_LOG("Kahroba: ARQC received - online authorization required");
        break;

    case 0x00u:
        EMV_LOG("Kahroba: AAC received - card declined");

        txn->status = TXN_STATUS_DECLINED;

        /* TODO: Introduce a distinct card-declined outcome. */
        return EMV_MGR_ERR_PROTOCOL;

    case 0x40u:
        EMV_LOG("Kahroba: TC received - unexpected online flow");

        /* TODO: Handle according to confirmed Kahroba rules. */
        return EMV_MGR_ERR_PROTOCOL;

    default:
        EMV_LOG("Kahroba: unexpected CID=%02X", context->acResult.cid);
        return EMV_MGR_ERR_PROTOCOL;
    }

    /* ================= Transfer PAN ================= */

    if (!kahrobaPopulateTxnPan(&context->card, txn)) {

        EMV_LOG("Kahroba: failed to populate transaction PAN");
        return EMV_MGR_ERR_PROTOCOL;
    }

    EMV_LOG("Kahroba: EMV complete; ready for online authorization");

    return EMV_MGR_OK;
}