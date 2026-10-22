#include "emvmgr.h"
#include "emv_dol.h"
#include <string.h>

/* PSE directory name for contactless application selection (14 ASCII bytes). */
static const uint8_t PPSE_NAME[14] = {'2', 'P', 'A', 'Y', '.', 'S', 'Y',
                                      'S', '.', 'D', 'D', 'F', '0', '1'};

static void wipe(void* memory, size_t len) {
    volatile uint8_t* p = (volatile uint8_t*)memory;
    while (len-- != 0u) {
        *p++ = 0u;
    }
}

/* Record outcome atomically; never mark an ARQC as a completed payment. */
static EmvStepResult finish(EmvMgr* mgr, EmvOutcome outcome, EmvError err) {
    mgr->result.outcome = outcome;
    mgr->result.error   = err;
    mgr->state          = EMV_STATE_FINISHED;
    mgr->active         = false;
    wipe(mgr->apdu_data, sizeof(mgr->apdu_data));
    wipe(mgr->response, sizeof(mgr->response));
    /* Event is a stack POD; receiver must post a value-copy, not a pointer. */
    if (mgr->on_completed != NULL) {
        EmvCompletedEvent event;
        event.transaction_id = mgr->result.transaction_id;
        event.session_id     = mgr->result.session_id;
        event.outcome        = outcome;
        event.error          = err;
        mgr->on_completed(mgr->event_context, &event);
    }
    return EMV_STEP_FINISHED;
}

static EmvStepResult failure(EmvMgr* mgr, EmvError err) {
    /* Failed/declined sessions must not leak stale partial card data. */
    emvTagDbClear(&mgr->result.tags);
    return finish(mgr, EMV_OUTCOME_FAILED, err);
}

EmvError emvMgrInit(EmvMgr* mgr, EmvTransport transport) {
    if (mgr == NULL || transport.exchange == NULL) {
        return EMV_ERR_ARGUMENT;
    }
    memset(mgr, 0, sizeof(*mgr));
    mgr->transport = transport;
    mgr->state     = EMV_STATE_IDLE;
    return EMV_OK;
}

EmvError emvMgrSetEventCallback(EmvMgr* mgr, EmvEventFn callback,
                                void* context) {
    if (mgr == NULL) {
        return EMV_ERR_ARGUMENT;
    }
    if (mgr->active) {
        return EMV_ERR_STATE;
    }
    mgr->on_completed  = callback;
    mgr->event_context = context;
    return EMV_OK;
}

void emvMgrReset(EmvMgr* mgr) {
    EmvTransport transport;
    EmvEventFn   callback;
    void*        event_context;
    if (mgr == NULL) {
        return;
    }
    /* Must be called only when no APDU call is in flight. */
    transport     = mgr->transport;
    callback      = mgr->on_completed;
    event_context = mgr->event_context;
    wipe(mgr, sizeof(*mgr));
    mgr->transport     = transport;
    mgr->on_completed  = callback;
    mgr->event_context = event_context;
    mgr->state         = EMV_STATE_IDLE;
}

void emvMgrCancel(EmvMgr* mgr) {
    if (mgr == NULL || !mgr->active) {
        return;
    }
    emvTagDbClear(&mgr->result.tags);
    (void)finish(mgr, EMV_OUTCOME_CANCELLED, EMV_OK);
}

EmvError emvMgrBegin(EmvMgr* mgr, const EmvStartParams* params) {
    static const struct {
        EmvTag tag;
        size_t length;
    } mandatory[] = {{0x9F02u, 6u}, {0x9F37u, 4u}, {0x9F1Au, 2u},
                     {0x5F2Au, 2u}, {0x9Au, 3u},   {0x9Cu, 1u},
                     {0x9F35u, 1u}, {0x95u, 5u},   {0x9F34u, 3u}};
    size_t i;
    if (mgr == NULL || params == NULL || mgr->transport.exchange == NULL ||
        params->terminal_tags == NULL || params->transaction_id == 0u ||
        params->session_id == 0u) {
        return EMV_ERR_ARGUMENT;
    }
    if (mgr->active) {
        return EMV_ERR_STATE;
    }
    if (params->terminal_tags == &mgr->result.tags) {
        return EMV_ERR_ARGUMENT;
    }
    /* Reject required missing values: caller must populate terminal context. */
    for (i = 0u; i < sizeof(mandatory) / sizeof(mandatory[0]); ++i) {
        const uint8_t* v   = NULL;
        size_t         len = 0u;
        if (!emvTagDbGet(params->terminal_tags, mandatory[i].tag, &v, &len) ||
            len != mandatory[i].length) {
            return EMV_ERR_MISSING_TAG;
        }
    }
    emvMgrReset(mgr);
    mgr->request = *params;
    /* No retained pointer to ephemeral caller tag storage. */
    mgr->request.terminal_tags = NULL;
    mgr->result.transaction_id = params->transaction_id;
    mgr->result.session_id     = params->session_id;
    mgr->result.outcome        = EMV_OUTCOME_NONE;
    mgr->result.error          = EMV_OK;
    mgr->result.tags           = *params->terminal_tags;
    mgr->state  = params->select_ppse_first ? EMV_STATE_SELECT_PPSE
                                            : EMV_STATE_SELECT_AID;
    mgr->active = true;
    return EMV_OK;
}

EmvOutcome emvMgrOutcome(const EmvMgr* mgr) {
    return mgr != NULL ? mgr->result.outcome : EMV_OUTCOME_FAILED;
}

const EmvResult* emvMgrResultView(const EmvMgr* mgr) {
    return mgr != NULL && mgr->state == EMV_STATE_FINISHED ? &mgr->result
                                                           : NULL;
}

EmvError emvMgrCopyResult(const EmvMgr* mgr, EmvResult* destination) {
    if (mgr == NULL || destination == NULL ||
        mgr->state != EMV_STATE_FINISHED) {
        return EMV_ERR_STATE;
    }
    *destination = mgr->result;
    return EMV_OK;
}

/* The HAL exchange runs synchronously on the caller's NFC worker/task. */
static EmvError exchange(EmvMgr* mgr, uint8_t cla, uint8_t ins, uint8_t p1,
                         uint8_t p2, const uint8_t* data, size_t data_len,
                         size_t* response_len) {
    EmvApduCommand  cmd;
    EmvApduResponse resp;
    int             rc;
    if (data_len > EMV_MAX_APDU_DATA || (data_len != 0u && data == NULL)) {
        return EMV_ERR_CAPACITY;
    }
    cmd.cla       = cla;
    cmd.ins       = ins;
    cmd.p1        = p1;
    cmd.p2        = p2;
    cmd.data      = data;
    cmd.data_len  = data_len;
    cmd.has_le    = true;
    cmd.le        = 256u;
    resp.data     = mgr->response;
    resp.capacity = sizeof(mgr->response);
    resp.length   = 0u;
    resp.sw1      = 0u;
    resp.sw2      = 0u;
    rc = mgr->transport.exchange(mgr->transport.context, &cmd, &resp);
    if (rc == -2) {
        return EMV_ERR_TIMEOUT;
    }
    if (rc != 0) {
        return EMV_ERR_TRANSPORT;
    }
    if (resp.length > resp.capacity) {
        return EMV_ERR_CAPACITY;
    }
    if (resp.sw1 != 0x90u || resp.sw2 != 0x00u) {
        return EMV_ERR_PROTOCOL;
    }
    *response_len = resp.length;
    return EMV_OK;
}

/** Verify that configured AID was advertised by the card PPSE directory. */
static EmvStepResult select_ppse(EmvMgr* mgr) {
    size_t   len;
    bool     present = false;
    EmvError err     = exchange(mgr, 0x00u, 0xA4u, 0x04u, 0x00u, PPSE_NAME,
                                sizeof(PPSE_NAME), &len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    err = emvTlvContains(mgr->response, len, 0x4Fu, EMV_KAHROBA_AID,
                         EMV_KAHROBA_AID_LEN, &present);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    if (!present) {
        return finish(mgr, EMV_OUTCOME_UNSUPPORTED, EMV_ERR_UNSUPPORTED);
    }
    mgr->state = EMV_STATE_SELECT_AID;
    return EMV_STEP_PROGRESS;
}

/** SELECT fixed AID, parse FCI and cache card PDOL/application metadata. */
static EmvStepResult select_aid(EmvMgr* mgr) {
    size_t         len;
    const uint8_t* df     = NULL;
    size_t         df_len = 0u;
    EmvError err = exchange(mgr, 0x00u, 0xA4u, 0x04u, 0x00u, EMV_KAHROBA_AID,
                            EMV_KAHROBA_AID_LEN, &len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    err = emvTlvCollectProtected(&mgr->result.tags, mgr->response, len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    /* Verify selection was not redirected to a different application. */
    if (emvTagDbGet(&mgr->result.tags, 0x84u, &df, &df_len)) {
        if (df_len != EMV_KAHROBA_AID_LEN ||
            memcmp(df, EMV_KAHROBA_AID, df_len) != 0) {
            return failure(mgr, EMV_ERR_PROTOCOL);
        }
    } else {
        err = emvTagDbSet(&mgr->result.tags, 0x84u, EMV_KAHROBA_AID,
                          EMV_KAHROBA_AID_LEN);
        if (err != EMV_OK) {
            return failure(mgr, err);
        }
    }
    mgr->state = EMV_STATE_GPO;
    return EMV_STEP_PROGRESS;
}

/** Decode EMV GPO template 80/77, copy and validate AFL entries. */
static EmvError decode_gpo(EmvMgr* mgr, size_t len) {
    EmvTagDb*      tags = &mgr->result.tags;
    const uint8_t* src  = NULL;
    size_t         n    = 0u;
    EmvError       err;
    if (len < 2u) {
        return EMV_ERR_PROTOCOL;
    }
    if (mgr->response[0] == 0x80u) {
        /* Format 1: 80 <len> <AIP 2 bytes> <AFL n bytes>. */
        if (mgr->response[1] != len - 2u || len < 4u) {
            return EMV_ERR_PROTOCOL;
        }
        err = emvTagDbSet(tags, 0x82u, mgr->response + 2u, 2u);
        if (err != EMV_OK) {
            return err;
        }
        err = emvTagDbSet(tags, 0x94u, mgr->response + 4u, len - 4u);
        if (err != EMV_OK) {
            return err;
        }
    } else if (mgr->response[0] == 0x77u) {
        err = emvTlvCollectProtected(tags, mgr->response, len);
        if (err != EMV_OK) {
            return err;
        }
    } else {
        return EMV_ERR_PROTOCOL;
    }
    if (!emvTagDbGet(tags, 0x82u, &src, &n) || n != 2u) {
        return EMV_ERR_PROTOCOL;
    }
    if (!emvTagDbGet(tags, 0x94u, &src, &n) || n > sizeof(mgr->afl) ||
        (n % 4u) != 0u) {
        return EMV_ERR_PROTOCOL;
    }
    if (n != 0u) {
        memcpy(mgr->afl, src, n);
    }
    mgr->afl_len      = n;
    mgr->afl_pos      = 0u;
    mgr->record_no    = 0u;
    mgr->records_read = 0u;
    {
        size_t i, count = 0u;
        for (i = 0u; i < n; i += 4u) {
            unsigned sfi           = mgr->afl[i] >> 3u;
            unsigned first         = mgr->afl[i + 1u];
            unsigned last          = mgr->afl[i + 2u];
            unsigned offline_count = mgr->afl[i + 3u];
            if (sfi < 1u || sfi > 30u || (mgr->afl[i] & 0x07u) != 0u ||
                first == 0u || last < first ||
                offline_count > last - first + 1u) {
                return EMV_ERR_PROTOCOL;
            }
            count += last - first + 1u;
            if (count > EMV_MAX_READ_RECORDS) {
                return EMV_ERR_CAPACITY;
            }
        }
    }
    return EMV_OK;
}

/** Build GPO command data with selected scheme profile and parse card response.
 */
static EmvStepResult get_processing_options(EmvMgr* mgr) {
    size_t   command_len = 0u, response_len = 0u;
    EmvError err = emvKahrobaBuildGpo(&mgr->result.tags, mgr->apdu_data,
                                      sizeof(mgr->apdu_data), &command_len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    if (command_len > sizeof(mgr->apdu_data)) {
        return failure(mgr, EMV_ERR_CAPACITY);
    }
    err = exchange(mgr, 0x80u, 0xA8u, 0x00u, 0x00u, mgr->apdu_data, command_len,
                   &response_len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    err = decode_gpo(mgr, response_len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    mgr->state =
        mgr->afl_len != 0u ? EMV_STATE_READ_RECORD : EMV_STATE_GENERATE_AC;
    return EMV_STEP_PROGRESS;
}

/** Read one AFL record per call, collecting validated primitive tags. */
static EmvStepResult read_record(EmvMgr* mgr) {
    size_t   len = 0u, pos;
    uint8_t  sfi, last;
    EmvError err;
    if (mgr->afl_pos >= mgr->afl_len) {
        mgr->state = EMV_STATE_GENERATE_AC;
        return EMV_STEP_PROGRESS;
    }
    pos  = mgr->afl_pos;
    sfi  = (uint8_t)(mgr->afl[pos] >> 3u);
    last = mgr->afl[pos + 2u];
    if (mgr->record_no == 0u) {
        mgr->record_no = mgr->afl[pos + 1u];
    }
    if (mgr->records_read >= EMV_MAX_READ_RECORDS) {
        return failure(mgr, EMV_ERR_CAPACITY);
    }
    err = exchange(mgr, 0x00u, 0xB2u, mgr->record_no,
                   (uint8_t)((sfi << 3u) | 0x04u), NULL, 0u, &len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    if (len == 0u || mgr->response[0] != 0x70u) {
        return failure(mgr, EMV_ERR_PROTOCOL);
    }
    err = emvTlvCollectProtected(&mgr->result.tags, mgr->response, len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    ++mgr->records_read;
    if (mgr->record_no == last) {
        mgr->record_no = 0u;
        mgr->afl_pos += 4u;
        if (mgr->afl_pos >= mgr->afl_len) {
            mgr->state = EMV_STATE_GENERATE_AC;
        }
    } else {
        ++mgr->record_no;
    }
    return EMV_STEP_PROGRESS;
}

/** Decode cryptogram response (template 80/77) and require CID/ATC/AC. */
static EmvError decode_ac(EmvMgr* mgr, size_t len, uint8_t* cid) {
    const uint8_t* v = NULL;
    size_t         n = 0u;
    EmvError       err;
    if (len < 2u) {
        return EMV_ERR_PROTOCOL;
    }
    if (mgr->response[0] == 0x80u) {
        /* Format 1: 80 length CID(1), ATC(2), AC(8), optional IAD. */
        if (mgr->response[1] != len - 2u || len < 13u) {
            return EMV_ERR_PROTOCOL;
        }
        err = emvTagDbSet(&mgr->result.tags, 0x9F27u, mgr->response + 2u, 1u);
        if (err != EMV_OK) {
            return err;
        }
        err = emvTagDbSet(&mgr->result.tags, 0x9F36u, mgr->response + 3u, 2u);
        if (err != EMV_OK) {
            return err;
        }
        err = emvTagDbSet(&mgr->result.tags, 0x9F26u, mgr->response + 5u, 8u);
        if (err != EMV_OK) {
            return err;
        }
        if (len > 13u) {
            err = emvTagDbSet(&mgr->result.tags, 0x9F10u, mgr->response + 13u,
                              len - 13u);
            if (err != EMV_OK) {
                return err;
            }
        }
    } else if (mgr->response[0] == 0x77u) {
        err = emvTlvCollectProtected(&mgr->result.tags, mgr->response, len);
        if (err != EMV_OK) {
            return err;
        }
    } else {
        return EMV_ERR_PROTOCOL;
    }
    if (!emvTagDbGet(&mgr->result.tags, 0x9F27u, &v, &n) || n != 1u) {
        return EMV_ERR_PROTOCOL;
    }
    *cid = v[0];
    if (!emvTagDbGet(&mgr->result.tags, 0x9F36u, &v, &n) || n != 2u ||
        !emvTagDbGet(&mgr->result.tags, 0x9F26u, &v, &n) || n != 8u) {
        return EMV_ERR_PROTOCOL;
    }
    return EMV_OK;
}

/** Build CDOL1 and request ARQC; report outcome without claiming approval. */
static EmvStepResult generate_ac(EmvMgr* mgr) {
    const uint8_t* cdol     = NULL;
    size_t         cdol_len = 0u, send_len = 0u, response_len = 0u;
    uint8_t        cid = 0u;
    EmvError       err;
    if (!emvTagDbGet(&mgr->result.tags, 0x8Cu, &cdol, &cdol_len)) {
        return finish(mgr, EMV_OUTCOME_UNSUPPORTED, EMV_ERR_UNSUPPORTED);
    }
    err = emvDolBuild(cdol, cdol_len, &mgr->result.tags, mgr->apdu_data,
                      sizeof(mgr->apdu_data), &send_len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    err = exchange(mgr, 0x80u, 0xAEu, 0x80u, /* ARQC requested */
                   0x00u, mgr->apdu_data, send_len, &response_len);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    err = decode_ac(mgr, response_len, &cid);
    if (err != EMV_OK) {
        return failure(mgr, err);
    }
    switch (cid & 0xC0u) {
    case 0x80u:
        return finish(mgr, EMV_OUTCOME_ONLINE_REQUIRED, EMV_OK);
    case 0x00u:
        emvTagDbClear(&mgr->result.tags);
        return finish(mgr, EMV_OUTCOME_DECLINED, EMV_OK);
    case 0x40u:
        return finish(mgr, EMV_OUTCOME_UNSUPPORTED, EMV_ERR_UNSUPPORTED);
    default:
        return failure(mgr, EMV_ERR_PROTOCOL);
    }
}

EmvStepResult emvMgrStep(EmvMgr* mgr) {
    if (mgr == NULL) {
        return EMV_STEP_REJECTED;
    }
    if (mgr->state == EMV_STATE_FINISHED) {
        return EMV_STEP_FINISHED;
    }
    if (!mgr->active) {
        return EMV_STEP_REJECTED;
    }
    switch (mgr->state) {
    case EMV_STATE_SELECT_PPSE:
        return select_ppse(mgr);
    case EMV_STATE_SELECT_AID:
        return select_aid(mgr);
    case EMV_STATE_GPO:
        return get_processing_options(mgr);
    case EMV_STATE_READ_RECORD:
        return read_record(mgr);
    case EMV_STATE_GENERATE_AC:
        return generate_ac(mgr);
    default:
        return failure(mgr, EMV_ERR_STATE);
    }
}

/**
 * Convenience blocking API: no worker-step scheduling or EMV event is needed.
 * Existing APDU parser/state-machine functions remain shared and unit tested.
 */
EmvError emvMgrRun(EmvMgr* mgr, const EmvStartParams* params,
                   EmvOutcome* outcome) {
    EmvError      err;
    EmvStepResult step;

    if (outcome == NULL) {
        return EMV_ERR_ARGUMENT;
    }
    *outcome = EMV_OUTCOME_NONE;
    err      = emvMgrBegin(mgr, params);
    if (err != EMV_OK) {
        return err;
    }

    for (;;) {
        step = emvMgrStep(mgr);
        if (step == EMV_STEP_PROGRESS) {
            continue;
        }
        if (step == EMV_STEP_REJECTED) {
            return EMV_ERR_STATE;
        }
        *outcome = mgr->result.outcome;
        return mgr->result.error;
    }
}
