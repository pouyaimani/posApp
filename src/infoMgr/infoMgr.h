#ifndef INFO_MGR_H_
#define INFO_MGR_H_

#include "state.h"
#include "ui/infoPage.h"
#include "txn.h"

typedef enum { INFO_RES_FINISH, INFO_RES_CANCEL } InfoResult;

typedef enum {
    INFO_MODE_NORMAL,
    INFO_MODE_DEV_INFO,
    INFO_MODE_DIGITAL_REC
} InfoMode;

typedef void (*InfoDone)(InfoResult res, void* arg);

typedef struct {
    State*   onFinish;
    State*   onCancel;
    InfoDone done;
    void*    userData;
} InfoFinishCtx;

typedef struct {
    State* state;
    void (*run)(InfoType_t type, InfoFinishCtx* ctx, const char* title,
                const char* body);
    void (*runDevInfo)(InfoFinishCtx* ctx);
    void (*runDigitalReceipt)(InfoFinishCtx* ctx, TxnData* txnData);
    InfoFinishCtx finishCtx;
    InfoMode      mode;
} InfoMgr;

InfoMgr* infoMgr();

extern InfoFinishCtx infoFinishCtx;

void infoDoneCommon(InfoResult result, void* userData);

void infoMgrsetCommon();

#endif