#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "event.h"
#include "assets.h"
#include "eventloop.h"
#include "network/network.h"
#include "iso8583.h"
#include "transmitter.h"
#include "phrases/phrases.h"


/******************************************************************/

STATE_DEF_ENTER(IsoTransmitter) {
    GOTO_NET_CONNNECT();
}

OOP_CTOR(IsoTransmitter, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(IsoTransmitter);

    self->connect = STATE_NET_CONNECT;
    self->send = STATE_NET_SEND;
    self->receive = STATE_NET_RECEIVE;
}

void GOTO_ISO_TRANSMITTER(NetSubTaskCtx_t *connect, 
                            NetSubTaskCtx_t *send,
                            NetSubTaskCtx_t *receive) {
    IsoTransmitter *isoTrns = STATE_TRNS_ISO;

    NetConnect *netConnect = STATE_NET_CONNECT;
    netConnect->ctx.onFailure = connect->onFailure;
    netConnect->ctx.onFailureCb = connect->onFailureCb;
    netConnect->ctx.onSucess = STATE_NET_SEND;
    netConnect->ctx.onSucessCb = connect->onSucessCb;
    netConnect->ctx.userDataOnFailure = connect->userDataOnFailure;
    netConnect->ctx.userDataOnSucess = connect->userDataOnSucess;

    NetSend *netSend = (NetSend *)STATE_NET_SEND;
    netSend->ctx.onFailure = send->onFailure;
    netSend->ctx.onFailureCb = send->onFailureCb;
    netSend->ctx.onSucess = STATE_NET_RECEIVE;
    netSend->ctx.onSucessCb = send->onSucessCb;
    netSend->ctx.userDataOnFailure = send->userDataOnFailure;
    netSend->ctx.userDataOnSucess = send->userDataOnSucess;

    NetReceive *netRec = (NetReceive *)STATE_NET_RECEIVE;
    netRec->ctx.onFailure = receive->onFailure;
    netRec->ctx.onFailureCb = receive->onFailureCb;
    netRec->ctx.onSucess = receive->onSucess;
    netRec->ctx.onSucessCb = receive->onSucessCb;
    netRec->ctx.userDataOnFailure = receive->userDataOnFailure;
    netRec->ctx.userDataOnSucess = receive->userDataOnSucess;
    SM_GOTO(STATE_TRNS_ISO);
}
