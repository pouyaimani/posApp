#include "pnaTmsProtocol.h"
#include <string.h>

#define PNA_SI 0x0Fu
#define PNA_SO 0x0Eu

static uint8_t frameLrc(const uint8_t* p, size_t size) {
    uint8_t lrc = 0;
    for (size_t i = 2; i + 1 < size; ++i)
        lrc ^= p[i];
    return lrc;
}

static void writeHeader(uint8_t* p, size_t size, PnaTmsCommand cmd) {
    size_t payload = size - 2;
    p[0]           = (uint8_t)(payload >> 8);
    p[1]           = (uint8_t)payload;
    p[2]           = PNA_SI;
    p[3]           = (uint8_t)('0' + (int)cmd / 10);
    p[4]           = (uint8_t)('0' + (int)cmd % 10);
    p[size - 2]    = PNA_SO;
    p[size - 1]    = frameLrc(p, size);
}

static bool parseDecimal(const uint8_t* p, size_t size, uint32_t* value) {
    uint32_t result = 0;
    if (!p || !value || size == 0)
        return false;
    for (size_t i = 0; i < size; ++i) {
        if (p[i] < '0' || p[i] > '9')
            return false;
        result = result * 10u + (uint32_t)(p[i] - '0');
    }
    *value = result;
    return true;
}

/*
 * The deployed PNA server may terminate the ASCII file-size value with NUL
 * (e.g. "1496248\0") or pad the remaining bytes with spaces. The value
 * must begin with at least one digit; padding is allowed only at the end.
 *
 * Keep this rule specific to the manifest size field. File count, sector
 * numbers, and data lengths still use strict, fixed-width decimal parsing.
 */
static bool parseManifestFileSize(const uint8_t* p, uint32_t* value) {
    uint32_t result         = 0;
    bool     sawDigit       = false;
    bool     paddingStarted = false;

    if (!p || !value)
        return false;

    for (size_t i = 0; i < 8u; ++i) {
        const uint8_t ch = p[i];

        if (ch >= (uint8_t)'0' && ch <= (uint8_t)'9' && !paddingStarted) {
            const uint32_t digit = (uint32_t)(ch - (uint8_t)'0');

            /* Bound the conversion as we parse; do not overflow uint32_t. */
            if (result > (PNA_TMS_MAX_FILE_SIZE - digit) / 10u)
                return false;

            result   = result * 10u + digit;
            sawDigit = true;
        } else if (sawDigit && (ch == 0x00u || ch == (uint8_t)' ')) {
            paddingStarted = true;
        } else {
            /* Reject leading padding, embedded padding, and other bytes. */
            return false;
        }
    }

    if (!sawDigit)
        return false;

    *value = result;
    return true;
}

void pnaTmsIdentityClear(PnaTmsIdentity* identity) {
    if (identity)
        memset(identity, ' ', sizeof(*identity));
}

PnaProtoResult pnaTmsFrameSize(const uint8_t prefix[2], size_t* frameSize) {
    if (!prefix || !frameSize)
        return PNA_PROTO_ARGUMENT;
    size_t size = 2u + ((size_t)prefix[0] << 8) + prefix[1];
    if (size < PNA_TMS_END_SIZE || size > PNA_TMS_MAX_FRAME_SIZE)
        return PNA_PROTO_FRAME;
    *frameSize = size;
    return PNA_PROTO_OK;
}

PnaProtoResult pnaTmsValidate(const uint8_t* frame, size_t len,
                              PnaTmsCommand* cmd) {
    size_t expected;
    if (!frame || !cmd)
        return PNA_PROTO_ARGUMENT;
    if (len < PNA_TMS_END_SIZE || len > PNA_TMS_MAX_FRAME_SIZE ||
        pnaTmsFrameSize(frame, &expected) != PNA_PROTO_OK || expected != len ||
        frame[2] != PNA_SI || frame[len - 2] != PNA_SO)
        return PNA_PROTO_FRAME;
    if (frame[len - 1] != frameLrc(frame, len))
        return PNA_PROTO_LRC;
    if (frame[3] < '0' || frame[3] > '9' || frame[4] < '0' || frame[4] > '9')
        return PNA_PROTO_COMMAND;
    int n = (frame[3] - '0') * 10 + frame[4] - '0';
    switch (n) {
    case PNA_CMD_RESUME:
    case PNA_CMD_MANIFEST:
    case PNA_CMD_FILE_REQUEST:
    case PNA_CMD_ERROR:
    case PNA_CMD_SECTOR:
    case PNA_CMD_END:
        *cmd = (PnaTmsCommand)n;
        return PNA_PROTO_OK;
    default:
        return PNA_PROTO_COMMAND;
    }
}

PnaProtoResult pnaTmsBuildResume(const PnaTmsIdentity* identity,
                                 uint8_t* output, size_t capacity,
                                 size_t* written) {
    if (!identity || !output || !written)
        return PNA_PROTO_ARGUMENT;
    if (capacity < PNA_TMS_RESUME_SIZE)
        return PNA_PROTO_CAPACITY;
    /* Do not memcpy struct: alignment / padding are not wire contracts. */
    size_t at = 5;
#define PUT_FIELD(field)                                                       \
    do {                                                                       \
        memcpy(output + at, identity->field, sizeof(identity->field));         \
        at += sizeof(identity->field);                                         \
    } while (0)
    PUT_FIELD(model);
    PUT_FIELD(serial);
    PUT_FIELD(terminal);
    PUT_FIELD(appVersion);
    PUT_FIELD(biosVersion);
    PUT_FIELD(hwInfo);
#undef PUT_FIELD
    if (at + 2 != PNA_TMS_RESUME_SIZE)
        return PNA_PROTO_FRAME;
    writeHeader(output, PNA_TMS_RESUME_SIZE, PNA_CMD_RESUME);
    *written = PNA_TMS_RESUME_SIZE;
    return PNA_PROTO_OK;
}

PnaProtoResult pnaTmsBuildSectorRequest(const uint8_t name[16],
                                        uint16_t sectorIndex, uint8_t* output,
                                        size_t capacity, size_t* written) {
    if (!name || !output || !written)
        return PNA_PROTO_ARGUMENT;
    if (capacity < PNA_TMS_FILE_REQ_SIZE)
        return PNA_PROTO_CAPACITY;
    if (sectorIndex >= PNA_TMS_MAX_SECTORS)
        return PNA_PROTO_FIELD;
    memcpy(output + 5, name, 16);
    /* The fixed-width sector field occupies wire offsets 21..24. */
    for (int i = 24; i >= 21; --i) {
        output[i] = (uint8_t)('0' + sectorIndex % 10u);
        sectorIndex /= 10u;
    }
    writeHeader(output, PNA_TMS_FILE_REQ_SIZE, PNA_CMD_FILE_REQUEST);
    *written = PNA_TMS_FILE_REQ_SIZE;
    return PNA_PROTO_OK;
}

static PnaProtoResult requireCommand(const uint8_t* frame, size_t len,
                                     PnaTmsCommand required) {
    PnaTmsCommand  actual;
    PnaProtoResult r = pnaTmsValidate(frame, len, &actual);
    if (r != PNA_PROTO_OK)
        return r;
    return actual == required ? PNA_PROTO_OK : PNA_PROTO_COMMAND;
}

PnaProtoResult pnaTmsParseManifest(const uint8_t* frame, size_t len,
                                   PnaTmsFile* files, size_t capacity,
                                   size_t* count) {
    if (!frame || !count || (!files && capacity))
        return PNA_PROTO_ARGUMENT;
    PnaProtoResult r = requireCommand(frame, len, PNA_CMD_MANIFEST);
    if (r != PNA_PROTO_OK)
        return r;
    if (len < 9)
        return PNA_PROTO_FRAME;
    uint32_t n;
    if (!parseDecimal(frame + 5, 2, &n) || n > PNA_TMS_MAX_FILES ||
        len != 9u + 40u * n)
        return PNA_PROTO_FIELD;
    if (n > capacity)
        return PNA_PROTO_CAPACITY;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* entry = frame + 7 + 40 * i;
        uint32_t       size;
        if (!parseManifestFileSize(entry + 16, &size))
            return PNA_PROTO_FIELD;
        memcpy(files[i].name, entry, 16);
        files[i].size = size;
        memcpy(files[i].md5, entry + 24, 16);
    }
    *count = n;
    return PNA_PROTO_OK;
}

PnaProtoResult pnaTmsParseSector(const uint8_t* frame, size_t len,
                                 PnaTmsSector* sector) {
    if (!frame || !sector)
        return PNA_PROTO_ARGUMENT;
    PnaProtoResult r = requireCommand(frame, len, PNA_CMD_SECTOR);
    if (r != PNA_PROTO_OK)
        return r;
    if (len < 15 || len > PNA_TMS_MAX_FRAME_SIZE)
        return PNA_PROTO_FRAME;
    uint32_t index, dataLen;
    if (!parseDecimal(frame + 5, 4, &index) ||
        !parseDecimal(frame + 9, 4, &dataLen) ||
        dataLen > PNA_TMS_SECTOR_SIZE || dataLen > len - 15u ||
        (dataLen != len - 15u && len != PNA_TMS_MAX_FRAME_SIZE))
        return PNA_PROTO_FIELD;
    sector->index  = (uint16_t)index;
    sector->length = (uint16_t)dataLen;
    sector->bytes  = frame + 13;
    return PNA_PROTO_OK;
}

PnaProtoResult pnaTmsParseError(const uint8_t* frame, size_t len,
                                PnaTmsServerError* error) {
    if (!frame || !error)
        return PNA_PROTO_ARGUMENT;
    PnaProtoResult r = requireCommand(frame, len, PNA_CMD_ERROR);
    if (r != PNA_PROTO_OK)
        return r;
    if (len < 9 || len > 73)
        return PNA_PROTO_FRAME;
    uint32_t declared;
    if (!parseDecimal(frame + 5, 2, &declared) || declared > 64)
        return PNA_PROTO_FIELD;
    const size_t actual = len - 9;
    if (declared > actual)
        return PNA_PROTO_FIELD;
    memcpy(error->lengthField, frame + 5, 2);
    memset(error->message, 0, sizeof(error->message));
    memcpy(error->message, frame + 7, actual);
    error->messageLength = actual;
    return PNA_PROTO_OK;
}

PnaProtoResult pnaTmsParseEnd(const uint8_t* frame, size_t len) {
    PnaProtoResult r = requireCommand(frame, len, PNA_CMD_END);
    if (r != PNA_PROTO_OK)
        return r;
    return len == PNA_TMS_END_SIZE ? PNA_PROTO_OK : PNA_PROTO_FRAME;
}
