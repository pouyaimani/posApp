#include "cell_t3Rtos.h"

#ifdef DEVICE_TRENDITT3RTOS

#include "sdkCellular.h"
#include "logger.h"

static CellErr_t translateSdkErr(int err) {
    CellErr_t cellErr;
    switch (err) {
    default:
        cellErr = CELL_ERR_OK;
        break;
    }
    return cellErr;
}

static void init(Cellular *self) {

}

static CellSigStrength_t getSignalStrength(Cellular *self) {
    uint8_t sig;
    CellSigStrength_t strength = CELL_SIGNAL_STRENGTH_INVALID;
    if (translateSdkErr(sdkCellularGetSignal(&sig)) == CELL_ERR_OK) {
        if (sig > 0 && sig < 5) {
            strength = CELL_SIGNAL_STRENGTH_0;
        } else if(sig > 5 && sig < 10) {
            strength = CELL_SIGNAL_STRENGTH_1;
        } else if (sig > 10 && sig < 15) {
            strength = CELL_SIGNAL_STRENGTH_2;
        } else if (sig > 15 && sig < 32) {
            strength = CELL_SIGNAL_STRENGTH_3;
        }
    }
    return strength;
}

static CellErr_t getSimInfo(Cellular *self, CellSimInfo *sinfo) {
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

static CellPPPStatus_t getPPPstatus(Cellular *self) {
    CellPPPStatus_t status = CELL_PPP_INVALID;
    int st = sdkCellularGetPPPStatus();
    // LOG_DEBUG("sdkCellularGetPPPStatus = %d", st);
    if (st == CELLULAR_PPP_ING) {
        status = CELL_PPP_DIALING;
    } else if (st == CELLULAR_PPP_SUCCESS) {
        status = CELL_PPP_SUCESS;
    } else if(st == CELLULAR_PPP_FAIL) {
        status = CELL_PPP_FAILURE;
    }
    return status;
}

static CellErr_t startPPPlogin(Cellular *self, const char *apn, 
                    const char *user, const char *pass, const char *number) {
    return translateSdkErr(sdkCellularStartPPPLogin(apn, user, pass, number));
}

static CellErr_t selectSim(uint8_t slot) {
    return translateSdkErr(sdkCellularSelectSim(slot));
}

static CellErr_t ussdInit(Cellular *self) {
    return translateSdkErr(sdkCellularUSSDInit());
}

static CellErr_t ussdSend(Cellular *self, char *req, uint8_t dsc) {
    return translateSdkErr(sdkCellularUSSDSend(req, dsc));
}

static CellErr_t ussdRec(Cellular *self, char *buff, size_t size) {
    return translateSdkErr(sdkCellularUSSDRecv(buff, size));
}

static CellErr_t ussdStop(Cellular *self) {
    return translateSdkErr(sdkCellularUSSDStop());
}

static CellErr_t ussdGetCharset(Cellular *self, char *cs, uint32_t len) {
    return translateSdkErr(sdkCellularUSSDGetCharset(cs, len));
}

static CellErr_t ussdSetCharset(Cellular *self, char *cs) {
    return translateSdkErr(sdkCellularUSSDSetCharset(cs));
}

static CellNeyType_t getNetType(Cellular *self) {
    int ret = sdkCellularGetNetType();
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

static CellErr_t getSimStatus(Cellular *self) {
    if (sdkCellularIoctl(SDK_CELLULAR_CTL_CHECKSIM, 0, 0) == SDK_CELLULAR_ERR_SIM) {
        return CELL_ERR_SIM_ERROR;
    }
    return CELL_ERR_OK;
}

OOP_CTOR(CellT3Rtos) {
    self->base.vtable.getNetType = getNetType;
    self->base.vtable.getPPPstatus = getPPPstatus;
    self->base.vtable.getSignalStrength = getSignalStrength;
    self->base.vtable.init = init;
    self->base.vtable.startPPPlogin = startPPPlogin;
    self->base.vtable.ussdGetCharset = ussdGetCharset;
    self->base.vtable.ussdGetCharset = ussdSetCharset;
    self->base.vtable.ussdInit = ussdInit;
    self->base.vtable.ussdRec = ussdRec;
    self->base.vtable.ussdSend = ussdSend;
    self->base.vtable.ussdStop = ussdStop;
    self->base.vtable.getSimInfo = getSimInfo;
    self->base.vtable.selectSim = selectSim;
    self->base.vtable.getSimStatus = getSimStatus;
}

#endif