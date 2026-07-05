#ifndef INPUT_MGR_H_
#define INPUT_MGR_H_

#include "state.h"
#include "inputCtrl.h"

typedef struct {
    State*           state;
    State*           next;
    State*           back;
    InputController* ctrl;
    void (*run)(InputCfg* cfg, State* back, State* next);
    void (*set)(InputType type, const char* in);
    void (*setOut)(char* oraw, char* oformated, uint16_t len);
    void (*reset)(InputType type);
    // pure non-formated input
    char* input;
    // formated input based on input mode
    char* finput;

    char*    oraw;
    char*    oformated;
    uint16_t outlen;
} InputMgr;

InputMgr* inmgr();

extern InputCfg inCfgEnterPin;

#endif