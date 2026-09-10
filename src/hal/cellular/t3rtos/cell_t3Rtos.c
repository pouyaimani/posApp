#include "cell_t3Rtos.h"

#ifdef DEVICE_TRENDITT3RTOS

#include "sdkCellular.h"
#include "logger.h"

static CellErr_t translateSdkErr(int err) {
    CellErr_t cellErr = CELL_ERR_OTHER;
    switch (err) {
    case SDK_CELLULAR_OK:
        cellErr = CELL_ERR_OK;
    default:
        break;
    }
    return cellErr;
}

static CellErr_t open(Cellular* self) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularOpen(NULL));
}

static CellErr_t init(Cellular* self) { return open(self); }

static CellErr_t close(Cellular* self) {
    VAR_UNUSED(self);
    int ret = sdkCellularClose();
    return translateSdkErr(ret);
}

static int8_t getImei(Cellular* dev, char* imei, size_t size) {
    int        result     = 0;
    ModuleInfo moduleInfo = {0};
    result                = sdkCellularGetModuleInfo(&moduleInfo);
    if (result == SDK_CELLULAR_OK) {
        snprintf(imei, size, "%s", moduleInfo.mImei);
        return ERR_OK;
    }
    return ERR_NOK;
}

static CellSigStrength_t getSignalStrength(Cellular* self) {
    VAR_UNUSED(self);
    uint8_t           sig;
    CellSigStrength_t strength = CELL_SIGNAL_STRENGTH_INVALID;
    if (translateSdkErr(sdkCellularGetSignal(&sig)) == CELL_ERR_OK) {
        if (sig > 0 && sig < 5) {
            strength = CELL_SIGNAL_STRENGTH_0;
        } else if (sig > 5 && sig < 10) {
            strength = CELL_SIGNAL_STRENGTH_1;
        } else if (sig > 10 && sig < 15) {
            strength = CELL_SIGNAL_STRENGTH_2;
        } else if (sig > 15 && sig < 32) {
            strength = CELL_SIGNAL_STRENGTH_3;
        }
    }
    return strength;
}

static CellErr_t getSimInfo(Cellular* self, CellSimInfo* sinfo) {
    VAR_UNUSED(self);
    SimInfo info;
    memset(sinfo->iccId, 0, sizeof(sinfo->iccId));
    memset(sinfo->imsi, 0, sizeof(sinfo->imsi));
    if (translateSdkErr(sdkCellularGetSimInfo(&info)) != CELL_ERR_OK) {
        return CELL_ERR_SIM_ERROR;
    }
    memcpy(sinfo->iccId, info.mIccid, sizeof(info.mIccid));
    memcpy(sinfo->imsi, info.mImsi, sizeof(info.mImsi));
    return CELL_ERR_OK;
}

static CellPPPStatus_t getPPPstatus(Cellular* self) {
    VAR_UNUSED(self);
    CellPPPStatus_t status = CELL_PPP_INVALID;
    int             st     = sdkCellularGetPPPStatus();
    // LOG_DEBUG("sdkCellularGetPPPStatus = %d", st);
    if (st == CELLULAR_PPP_ING) {
        status = CELL_PPP_DIALING;
    } else if (st == CELLULAR_PPP_SUCCESS) {
        status = CELL_PPP_SUCESS;
    } else if (st == CELLULAR_PPP_FAIL) {
        status = CELL_PPP_FAILURE;
    }
    return status;
}

static CellErr_t startPPPlogin(Cellular* self, const char* apn,
                               const char* user, const char* pass,
                               const char* number) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularStartPPPLogin(apn, user, pass, number));
}

static CellErr_t selectSim(uint8_t slot) {
    return translateSdkErr(sdkCellularSelectSim(slot));
}

static CellErr_t ussdInit(Cellular* self) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularUSSDInit());
}

static CellErr_t ussdSend(Cellular* self, char* req, uint8_t dsc) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularUSSDSend(req, dsc));
}

static CellErr_t ussdRec(Cellular* self, char* buff, size_t size) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularUSSDRecv(buff, size));
}

static CellErr_t ussdStop(Cellular* self) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularUSSDStop());
}

static CellErr_t ussdGetCharset(Cellular* self, char* cs, uint32_t len) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularUSSDGetCharset(cs, len));
}

static CellErr_t ussdSetCharset(Cellular* self, char* cs) {
    VAR_UNUSED(self);
    return translateSdkErr(sdkCellularUSSDSetCharset(cs));
}

static CellNeyType_t getNetType(Cellular* self) {
    VAR_UNUSED(self);
    int           ret     = sdkCellularGetNetType();
    CellNeyType_t netType = CELL_NET_TYPE_INVALID;
    if (ret == CELLULAR_NET_2G) {
        netType = CELL_NET_TYPE_2G;
    } else if (ret == CELLULAR_NET_3G) {
        netType = CELL_NET_TYPE_3G;
    } else if (ret == CELLULAR_NET_4G) {
        netType = CELL_NET_TYPE_4G;
    }
    return netType;
}

static SimStatus_t simSt;

static SimStatus_t getSimStatus(Cellular* self) {
    VAR_UNUSED(self);
    return simSt;
}

static void checkSimStatus(Cellular* self) {
    VAR_UNUSED(self);
    int ret = sdkCellularIoctl(SDK_CELLULAR_CTL_CHECKSIM, 0, 0);
    LOG_TRACE("checkSimStatus: sdkCellularIoctl() ret = %d", ret);
    if (ret == SDK_CELLULAR_ERR_BASE || ret == SDK_CELLULAR_ERR_SIM) {
        simSt = SIM_STATUS_ERR;
        return;
    }
    simSt = SIM_STATUS_OK;
}

OOP_CTOR(CellT3Rtos) {
    self->base.vtable.init              = init;
    self->base.vtable.open              = open;
    self->base.vtable.close             = close;
    self->base.vtable.getNetType        = getNetType;
    self->base.vtable.getPPPstatus      = getPPPstatus;
    self->base.vtable.getSignalStrength = getSignalStrength;
    self->base.vtable.startPPPlogin     = startPPPlogin;
    self->base.vtable.ussdGetCharset    = ussdGetCharset;
    self->base.vtable.ussdGetCharset    = ussdSetCharset;
    self->base.vtable.ussdInit          = ussdInit;
    self->base.vtable.ussdRec           = ussdRec;
    self->base.vtable.ussdSend          = ussdSend;
    self->base.vtable.ussdStop          = ussdStop;
    self->base.vtable.getSimInfo        = getSimInfo;
    self->base.vtable.selectSim         = selectSim;
    self->base.vtable.getSimStatus      = getSimStatus;
    self->base.vtable.getImei           = getImei;
    self->base.vtable.checkSimStatus    = checkSimStatus;
}

#endif