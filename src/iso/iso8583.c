#include "iso8583.h"
#include "oscar/dl_iso8583.h"
#include "sys/sys.h"
#include "common.h"
#include "logger.h"
#include "error.h"
#include "utility/utility.h"

static Iso8583 *__iso8583;

#define SELF Iso8583 *self = __iso8583

/* ================= Iso8583 Methods ================= */

static void init(void)
{
    SELF;
    DL_ISO8583_DEFS_1993_GetHandler(&self->handler);
    DL_ISO8583_MSG_Init(NULL, 0, &self->msg);
}

static void reset() {
    SELF;
    DL_ISO8583_MSG_Free(&self->msg);
    DL_ISO8583_MSG_Init(NULL, 0, &self->msg);

    memset(self->mti, 0, sizeof(self->mti));
}

static void destroy() {
    SELF;
    DL_ISO8583_MSG_Free(&self->msg);
}

/* ===== MTI ===== */

static IsoStatus_t setMTI(const char *mti) {
    RETURN_VALUE_IF_NULL(mti, ;, ISO_ERR_INPUT);
    SELF;
    if (!mti || strlen(mti) != 4)
        return ISO_ERR_INVALID_FIELD;

    memcpy(self->mti, mti, 4);
    self->mti[4] = '\0';

    return ISO_OK;
}

static const char* getMTI(void)
{
    return __iso8583->mti;
}

/* ===== Fields ===== */

IsoStatus_t setStr(uint16_t field,
                          const void *data) {
    RETURN_VALUE_IF_NULL(data, ;, ISO_ERR_INPUT);
    SELF;
    if (field == 0 || field > ISO_MAX_FIELDS)
        return ISO_ERR_INVALID_FIELD;

    if (DL_ISO8583_MSG_SetField_Str( field,
                (const uint8_t*)data,
                &self->msg) != 0)
            return ISO_ERR_PACK;

    return ISO_OK;
}

IsoStatus_t setBin(uint16_t field,
                          const void *data,
                          size_t len) {
    RETURN_VALUE_IF_NULL(data, ;, ISO_ERR_INPUT);
    SELF;
    if (field == 0 || field > ISO_MAX_FIELDS)
        return ISO_ERR_INVALID_FIELD;

    if (DL_ISO8583_MSG_SetField_Bin(
            field,
            (const uint8_t*)data,
            len,
            &self->msg) != 0)
        return ISO_ERR_PACK;

    return ISO_OK;
}

IsoStatus_t getStr(uint16_t field,
                          void *out) {
    RETURN_VALUE_IF_NULL(out, ;, ISO_ERR_INPUT);
    SELF;

    uint8_t *ptr = NULL;
    uint16_t flen = 0;

    int rc;

    rc = DL_ISO8583_MSG_GetField_Str(field,
                                         &self->msg,
                                         &ptr);
    flen = strlen((char*)ptr);

    if (rc != 0)
        return ISO_ERR_INVALID_FIELD;

    memcpy(out, ptr, flen);
    return ISO_OK;
}

IsoStatus_t getBin(uint16_t field,
                          void *out,
                          size_t *len)
{
    RETURN_VALUE_IF_NULL(out, ;, ISO_ERR_INPUT);
    RETURN_VALUE_IF_NULL(len, ;, ISO_ERR_INPUT);
    SELF;
    uint8_t *ptr = NULL;
    uint16_t flen = 0;
    int rc;

    rc = DL_ISO8583_MSG_GetField_Bin(field,
                                         &self->msg,
                                         &ptr,
                                         &flen);


    if (rc != 0)
        return ISO_ERR_INVALID_FIELD;

    if (*len < flen)
        return ISO_ERR_BUFFER_TOO_SMALL;

    memcpy(out, ptr, flen);
    *len = flen;

    return ISO_OK;
}

#ifdef USE_DUMP

void dump(const DL_ISO8583_HANDLER *iHandler,
                         const DL_ISO8583_MSG *iMsg)
{
    DL_UINT16 i;
    char line[256];

    LOG_DEBUG("--------------- ISO8583 MSG DUMP ---------------");

    for (i = 0; i < iHandler->fieldItems; i++)
    {
        if (iMsg->field[i].ptr != NULL)
        {
            snprintf(line,
                     sizeof(line),
                     "[%03d] %s",
                     (int)i,
                     iMsg->field[i].ptr);

            LOG_DEBUG(line);
        }
    }

    LOG_DEBUG("------------------------------------------------");
}

void dumpRaw(const uint8_t *data, size_t len)
{
    char line[128];
    size_t pos;

    LOG_DEBUG("---------- ISO8583 RAW DUMP (%u bytes) ----------", (unsigned)len);

    for (size_t i = 0; i < len; i += 16)
    {
        pos = 0;

        for (size_t j = 0; j < 16 && (i + j) < len; j++)
        {
            pos += snprintf(
                line + pos,
                sizeof(line) - pos,
                "%02X ",
                data[i + j]);
        }

        LOG_DEBUG("%s", line);
    }

    LOG_DEBUG("------------------------------------------------");
}

#endif

static IsoStatus_t addHeader(uint8_t *buffer, const uint8_t *packedData,
                     size_t *packedLen, IsoHeaderData_t *hd) {
    RETURN_VALUE_IF_NULL(buffer, ;, ISO_ERR_INPUT);
    RETURN_VALUE_IF_NULL(packedData, ;, ISO_ERR_INPUT);
    RETURN_VALUE_IF_NULL(packedLen, ;, ISO_ERR_INPUT);
    RETURN_VALUE_IF_NULL(hd, ;, ISO_ERR_INPUT);
    /* create heaer */
    DEFINE_STRING(nii, 2);
    memcpy(nii, (unsigned char *)&hd->nii, 2);
	buffer[2] = 0x60;
	buffer[3] = nii[1]; // 0x01;
	buffer[4] = nii[0]; // 0x18;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
    memcpy(buffer + 7, packedData, *packedLen);
	*packedLen += 5;

	buffer[0] = *packedLen / 256;
	buffer[1] = *packedLen % 256;
    *packedLen += 2;
#if USE_DUMP
    dumpRaw(buffer, *packedLen);
#endif
    return ISO_OK;
}

/* ===== PACK ===== */

IsoStatus_t pack(const uint8_t *data,
                       size_t *outlen)
{
    SELF;
    RETURN_VALUE_IF_NULL(data, ;, ISO_ERR_INPUT);
    RETURN_VALUE_IF_NULL(outlen, ;, ISO_ERR_INPUT);
    if (self->mti[0] == 0)
        return ISO_ERR_PACK;

    RETURN_VALUE_IF_NOT(DL_ISO8583_MSG_SetField_Str(
                        0, (const uint8_t*)self->mti,
                            &self->msg), 0, ;, ISO_ERR_PACK);

    int out_len = 0;
    RETURN_VALUE_IF_NOT(DL_ISO8583_MSG_Pack(&self->handler,
                            &self->msg,
                            data,
                            &out_len), 0, ;, ISO_ERR_PACK);

    *outlen = out_len;

#ifdef USE_DUMP
    dump(&self->handler, &self->msg);
#endif
    return ISO_OK;
}

/* ===== PARSE ===== */

IsoStatus_t parse(const uint8_t *data,
                       size_t len) {
    SELF;
    RETURN_VALUE_IF_NULL(data, ;, ISO_ERR_INPUT);
    reset();
#ifdef USE_DUMP
    dumpRaw(data, len);
#endif
    uint16_t packedSize = data[0] * 256 + data[1];
	packedSize = packedSize - 5;				// without header
    RETURN_VALUE_IF_NOT(DL_ISO8583_MSG_Unpack(&self->handler,
                              data + 7,
                              packedSize,
                              &self->msg), 0, ;, ISO_ERR_PARSE);

    uint8_t *ptr = NULL;
    uint16_t flen = 0;

    if (DL_ISO8583_MSG_GetField_Str(0,
                                   &self->msg,
                                   &ptr) == 0) {

        memcpy(self->mti, ptr, 4);
        self->mti[4] = '\0';
    }

#ifdef USE_DUMP
    dump(&self->handler, &self->msg);
#endif

    return ISO_OK;
}

OOP_CTOR(Iso8583) {
    self->init          = init;
    self->destroy       = destroy;
    self->reset         = reset;
    self->setMTI        = setMTI;
    self->getMTI        = getMTI;
    self->setBin        = setBin;
    self->setStr        = setStr;
    self->getBin        = getBin;
    self->getStr        = getStr;
    self->pack          = pack;
    self->parse         = parse;
    self->addHeader     = addHeader;
    init();
}

Iso8583 *iso8583() {
    CALL_ONCE(
        __iso8583 = MEM_ALLOC(sizeof(Iso8583));
        OOP_CALL_CTOR(Iso8583, __iso8583);
    );
    return __iso8583;
}