#ifndef EMV_MGR_H
#define EMV_MGR_H

#include <stddef.h>
#include <stdint.h>
#include "emv_dol.h"

#include "emv_transport.h"

typedef enum {
    EMV_MGR_OK = 0,
    EMV_MGR_ERR_ARGUMENT,
    EMV_MGR_ERR_STATE,
    EMV_MGR_ERR_TRANSPORT,
    EMV_MGR_ERR_TIMEOUT,
    EMV_MGR_ERR_CARD_STATUS,
    EMV_MGR_ERR_PROTOCOL,
    EMV_MGR_ERR_UNSUPPORTED
} EmvMgrError;

typedef enum {
    EMV_MGR_IDLE = 0,
    EMV_MGR_PPSE_SELECTED,
    EMV_MGR_AID_SELECTED,
    EMV_MGR_GPO_COMPLETED,
    EMV_MGR_FAILED,
    EMV_MGR_RECORDS_READ,
    EMV_MGR_AC_GENERATED
} EmvMgrState;

typedef struct {
    uint8_t cid;    /* 9F27 - Cryptogram Information Data */
    uint8_t atc[2]; /* 9F36 - Application Transaction Counter */
    uint8_t ac[8];  /* 9F26 - Application Cryptogram */

    uint8_t iad[32]; /* 9F10 - Issuer Application Data */
    size_t  iadLen;
} EmvGenerateAcResult;

typedef bool (*EmvDolValueFn)(void* context, uint32_t tag, uint8_t* output,
                              size_t requiredLength);

/**
 * Called after a successful READ RECORD.
 *
 * data points to the contents of template 70.
 * The pointer remains valid only during this callback.
 *
 * Return false if the record cannot be processed.
 */
typedef bool (*EmvRecordFn)(void* context, uint8_t sfi, uint8_t recordNumber,
                            const uint8_t* data, size_t length);

/**
 * Public EMV manager interface.
 *
 * The manager does not own the PICC reader.
 * The transaction/session coordinator owns reader cleanup.
 */
typedef struct {
    EmvMgrError (*init)(EmvTransport transport);

    EmvMgrError (*selectPpse)(const uint8_t* aid, size_t aidLen);

    EmvMgrError (*selectAid)(const uint8_t* aid, size_t aidLen);

    EmvMgrError (*getProcessingOptions)(EmvDolValueFn provider,
                                        void*         providerContext);

    EmvMgrError (*readRecords)(EmvRecordFn callback, void* userData);

    EmvMgrError (*generateAc)(const uint8_t* cdol1, size_t cdol1Len,
                              EmvDolValueFn provider, void* providerContext,
                              EmvGenerateAcResult* result);

    EmvMgrState (*getState)(void);

    void (*reset)(void);
} EmvMgr;

/** Return the application-wide EMV manager singleton. */
EmvMgr* emvMgr(void);

#endif