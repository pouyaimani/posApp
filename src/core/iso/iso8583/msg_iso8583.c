#include "msg_iso8583.h"
#include "oscar/dl_iso8583.h"

#if MSG_STANDARD == ISO8583

#define DEFINE_FIELD(id, name, fmt, len_type, max_len) \
    { id, name, fmt, len_type, max_len },

IsofieldDef_t isoFields[] = {
    ISO8583_FIELD_TABLE(DEFINE_FIELD)
};

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
            if (isoFields[i].format == FMT_B) {
                if (DL_ISO8583_MSG_GetField_Bin(i, &isoMsg, &ptr, &size) == 0) {
                    memcpy(self->element[i].data, ptr, size);
                }
            } else {
                if (DL_ISO8583_MSG_GetField_Str(i, &isoMsg, &ptr) == 0 ) {
                    strcpy(self->element[i].data, ptr);
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
            if (isoFields[i].format == FMT_B) {
                DL_ISO8583_MSG_SetField_Bin(i, (const uint8_t *)self->element[i].data, 
                                                self->element[i].len, &isoMsg);
            } else {
                DL_ISO8583_MSG_SetField_Str(i, (const uint8_t *)self->element[i].data, &isoMsg);
            }
        }
    }
    return MSG_ERR_OK;
}

static void setAmount(Packer *self, const char *amt) {
    snprintf(self->element[ELEMENT_AMOUNT_TRANSACTION].data,
                isoFields[ELEMENT_AMOUNT_TRANSACTION].maxLen + 1,
                    "%s", amt);
}

static const char *getAmount(Packer *self) {
    return self->element[ELEMENT_AMOUNT_TRANSACTION].data;
}

static void setPan(Packer *self, const char *pan) {
    snprintf(self->element[ELEMENT_PAN].data,
                isoFields[ELEMENT_PAN].maxLen + 1,
                    "%s", pan);    
}

static const char *getPan(Packer *self) {
    return self->element[ELEMENT_PAN].data;
}

static void setCardPwd(Packer *self, const char *pwd) {
    snprintf(self->element[ELEMENT_PIN_DATA].data,
                isoFields[ELEMENT_PIN_DATA].maxLen + 1,
                    "%s", pwd);  
}

static const char *getCardPwd(Packer *self) {
    return self->element[ELEMENT_PIN_DATA].data;
}

static void setDateTime(Packer *self, const char *dt) {
    snprintf(self->element[ELEMENT_TRANSMISSION_DATE_TIME].data,
                isoFields[ELEMENT_TRANSMISSION_DATE_TIME].maxLen + 1,
                    "%s", dt);  
}

static const char *getDateTime(Packer *self) {
    return self->element[ELEMENT_TRANSMISSION_DATE_TIME].data;
}

static void setTerminalId(Packer *self, const char *id) {
}

static const char *getTerminalId(Packer *self) {
}

static void setRRN(Packer *self, const char *rrn) {
}

static const char *getRRN(Packer *self) {
}

static void setTrack2(Packer *self, const char *t2) {
}

static const char *getTrack2(Packer *self) {
}

static void setMac(Packer *self, uint8_t *mac, size_t len) {
}

OOP_CTOR(Iso8583Packer) {
    self->base.vtable.pack = pack;
    self->base.vtable.setAmount = setAmount;
    self->base.vtable.getAmount = getAmount;
    self->base.vtable.setPan = setPan;
    self->base.vtable.getPan = getPan;
    self->base.vtable.setCardPwd = setCardPwd;
    self->base.vtable.getCardPwd = getCardPwd;
    self->base.vtable.setDateTime = setDateTime;
    self->base.vtable.getDateTime = getDateTime;
    self->base.vtable.setTerminalId = setTerminalId;
    self->base.vtable.getTerminalId = getTerminalId;
    self->base.vtable.setRRN = setRRN;
    self->base.vtable.getRRN = getRRN;
    self->base.vtable.setTrack2 = setTrack2;
    self->base.vtable.getTrack2 = getTrack2;
    self->base.vtable.setMac = setMac;
    DL_ISO8583_DEFS_1993_GetHandler(&isoHandler);
}


#endif