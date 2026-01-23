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
        return ISO_MSG_ERR_PARSE_FAILED;
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
    // return PARSER_ERR_COMPLETED;
}

OOP_CTOR(Iso8583Parser) {
    self->base.vtable.parse = parse;
    DL_ISO8583_DEFS_1993_GetHandler(&isoHandler);
}

static IsoMsgErr_t pack(const char *data) {
    // DL_ISO8583_MSG_Init(NULL, 0, &packager.reqMsg);
    // for (uint8_t i = 0 ; i < MSG_FIELDS_CONUT ; i++) {
    //     if (elements[i].isFilled()) {
    //         switch (ISO_MSG_TYPE[i])
    //         {
    //         case ISO_MSG_STR:
    //             // PLOG_DEBUG << "field[" << (uint16_t)i << "] = " << elements[i].asString();
    //             DL_ISO8583_MSG_SetField_Str(i, (const uint8_t *)elements[i].asString().data(), &packager.reqMsg);
    //             break;
    //         case ISO_MSG_BYTE_ARRAY:
    //             // PLOG_DEBUG << "field[" << (uint16_t)i << "] = " << plog::hexdump(elements[i].asByteArray().data(), 
    //             // elements[i].asByteArray().size());
    //             DL_ISO8583_MSG_SetField_Bin(i, (const DL_UINT8 *)elements[i].asByteArray().data(), 
    //             elements[i].asByteArray().size(), &packager.reqMsg);
    //             break;
    //         default:
    //             break;
    //         }
    //     }
    // }
    // packager.pack();
    // return packager;
}

OOP_CTOR(Iso8583Packer) {
    self->base.vtable.pack = pack;
    DL_ISO8583_DEFS_1993_GetHandler(&isoHandler);
}


#endif