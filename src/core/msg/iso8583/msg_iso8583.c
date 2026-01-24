#include "msg_iso8583.h"
#include "oscar/dl_iso8583.h"

#if MSG_STANDARD == ISO8583

DL_ISO8583_HANDLER isoHandler;
DL_ISO8583_MSG     isoMsg;

static IsoMsgErr_t parse(Parser *self, const char *data) {
    DL_ISO8583_MSG_Init(NULL, 0, &isoMsg);
    // for (size_t i = isoStartIdx ; i < rawData.size() ; i++) {
    //     byteArray.append(rawData[i]);
    // }
    if (DL_ISO8583_MSG_Unpack(&isoHandler, self->buffer, sizeof(self->buffer), &isoMsg) != 0) {
        return MSG_ERR_PARSE_FAILED;
    }
    DL_UINT8 *ptr = NULL;
    DL_UINT16 size;
    for (uint8_t i = 2 ; i < MSG_FIELDS_CONUT ; i++) {
        memset(self->element[i].data, 0, MSG_FIELD_SIZE);
        if (DL_ISO8583_MSG_HaveField(i, &isoMsg)) {
            if (ISO_MSG_TYPE[i] == ISO_MSG_STR) {
                if (DL_ISO8583_MSG_GetField_Str(i, &isoMsg, &ptr) == 0 ) {
                    strcpy(self->element[i].data, ptr);
                }
            } else if (ISO_MSG_TYPE[i] == ISO_MSG_BYTE) {
                if (DL_ISO8583_MSG_GetField_Bin(i, &isoMsg, &ptr, &size) == 0) {
                    memcpy(self->element[i].data, ptr, size);
                }
            }
        }
    }
    DL_ISO8583_MSG_Free(&isoMsg);
    return MSG_ERR_OK;
}

OOP_CTOR(Iso8583Parser) {
    self->base.vtable.parse = parse;
    DL_ISO8583_DEFS_1993_GetHandler(&isoHandler);
}

static IsoMsgErr_t pack(Packer *self,const char *data) {
    DL_ISO8583_MSG_Init(NULL, 0, &isoMsg);
    for (uint8_t i = 0 ; i < MSG_FIELDS_CONUT ; i++) {
        if (self->element[i].isFilled(i)) {
            switch (ISO_MSG_TYPE[i])
            {
            case ISO_MSG_STR:
                DL_ISO8583_MSG_SetField_Str(i, (const uint8_t *)self->element[i].data, &isoMsg);
                break;
            case ISO_MSG_BYTE:
                DL_ISO8583_MSG_SetField_Bin(i, (const uint8_t *)self->element[i].data, 
                                                self->element[i].len, &isoMsg);
                break;
            default:
                break;
            }
        }
    }
    return MSG_ERR_OK;
}

OOP_CTOR(Iso8583Packer) {
    self->base.vtable.pack = pack;
    DL_ISO8583_DEFS_1993_GetHandler(&isoHandler);
}


#endif