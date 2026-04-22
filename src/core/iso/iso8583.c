#include "iso8583.h"
#include "oscar/dl_iso8583.h"
#include "dev/dev.h"

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

    self->buffer_len = 0;
    memset(self->mti, 0, sizeof(self->mti));
}

static void destroy() {
    SELF;
    DL_ISO8583_MSG_Free(&self->msg);
}

/* ===== MTI ===== */

static IsoStatus_t setMTI(const char *mti)
{
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
    if (!out)
        return ISO_ERR_INVALID_FIELD;
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
    if (!out || !len)
        return ISO_ERR_INVALID_FIELD;
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

/* ===== PACK ===== */

IsoStatus_t pack()
{
    SELF;
    if (self->mti[0] == 0)
        return ISO_ERR_PACK;

    if (DL_ISO8583_MSG_SetField_Str(
            0,
            (const uint8_t*)self->mti,
            &self->msg) != 0)
        return ISO_ERR_PACK;

    int out_len = 0;

    if (DL_ISO8583_MSG_Pack(&self->handler,
                            &self->msg,
                            self->buffer,
                            &out_len) != 0)
        return ISO_ERR_PACK;

    self->buffer_len = out_len;

    return ISO_OK;
}

/* ===== PARSE ===== */

IsoStatus_t parse(const uint8_t *data,
                       size_t len)
{
    SELF;
    reset();

    if (DL_ISO8583_MSG_Unpack(&self->handler,
                              data,
                              len,
                              &self->msg) != 0)
        return ISO_ERR_PARSE;

    uint8_t *ptr = NULL;
    uint16_t flen = 0;

    if (DL_ISO8583_MSG_GetField_Str(0,
                                   &self->msg,
                                   &ptr) == 0) {

        memcpy(self->mti, ptr, 4);
        self->mti[4] = '\0';
    }

    return ISO_OK;
}

OOP_CTOR(Iso8583) {
    self->init    = init;
    self->destroy  = destroy;
    self->reset    = reset;
    self->setMTI   = setMTI;
    self->getMTI   = getMTI;
    self->setBin = setBin;
    self->setStr = setStr;
    self->getBin = getBin;
    self->getStr = getStr;
    self->pack     = pack;
    self->parse    = parse;
    init();
}

Iso8583 *iso8583() {
    CALL_ONCE(
        __iso8583 = GET_MEM(sizeof(Iso8583));
        OOP_CALL_CTOR(Iso8583, __iso8583);
    );
    return __iso8583;
}