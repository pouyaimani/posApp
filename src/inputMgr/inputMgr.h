#ifndef INPUT_MGR_H_
#define INPUT_MGR_H_

#include "state.h"
#include "inputCtrl.h"
#include "timeout.h"

typedef struct {
    State*     onFinish;
    State*     onCancel;
    CallBack_t cb;
    void*      userData;
} InputFinishCtx;

typedef struct {
    State*           state;
    InputController* ctrl;
    void (*run)(InputCfg* cfg, InPutComplete cb, void* cbData);
    void (*set)(InputType type, const char* in);
    void (*setDefault)(InputType type, const char* in);
    void (*commit)(char* oraw, char* oformated, uint16_t len);
    void (*reset)(InputType type);
    // pure non-formated input
    char* input;
    // formated input based on input mode
    char* finput;

    char*    oraw;
    char*    oformated;
    uint16_t outlen;

    InPutComplete cb;
    void*         cbData;
} InputMgr;

InputMgr* inmgr();

extern InputCfg inCfgEnterPin;

extern InputFinishCtx inFinishCtx;

void inputComplete(InputResult result, void* userData);

#endif