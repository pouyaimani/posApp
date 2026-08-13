#include "txnFlow/txnFLow.h"
#include "settings/settings.h"
#include "iso/iso8583.h"
#include "txnCommon.h"
#include "transaction.h"
#include "supporter.h"
#include "states.h"

static TxnFlow* gflow;

static void cfgDone(TxnFlow* flow, const TxnFlowStatus* st) {
    if (st->result == TXN_FLOW_SUCCESS && st->code == 0) {
        settings()->save();
    }
    Supporter* state = (Supporter*)STATE_SUPPORTER;
    commonDone(flow, st, state->substate.supervisor->substate.configuration,
               state->substate.supervisor->substate.configuration, true);
}

static const uint8_t isoFeilds[] = {
    ELEMENT_PROCESSING_CODE, ELEMENT_STAN, ELEMENT_TIME_LOCAL_TRANSACTION,
    ELEMENT_DATE_LOCAL_TRANSACTION, ELEMENT_NETWORK_INTL_ID,
    // ELEMENT_ACQUIRING_INSTITUTION_ID,
    ELEMENT_TERMINAL_ID, ELEMENT_ADDITIONAL_DATA_PRIVATE,
    ELEMENT_SECURITY_CONTROL_INFO, ELEMENT_MAC};

const TxnFlowConfig cfgTxn = {

    TXN_FLOW_COMMON,

    .type = TXN_CFG,

    .mti = MTI_AUTH_REQ,

    .prcode = PRC_CFG,

    .feilds = isoFeilds,

    .feildsCnt = sizeof(isoFeilds),

    .needSettlement = false,

    .compose = NULL,

    .done = cfgDone,

    .digitalReceipt = false};

STATE_DEF_ENTER(Config) {
    DEFINE_STRING(ip, 32);
    normalizeIp(settings()->server.mainServerIp, ip, sizeof(ip));
    txnRun(gflow, state, ip, settings()->server.mainServerPort, &cfgTxn);
}

OOP_CTOR(Config, State* parent, const char* name) {
    OOP_CALL_CTOR(Transaction, self, parent, name);
    self->base.state.vtable.enter = STATE_ENTER(Config);

    gflow = self->base.flow;
}