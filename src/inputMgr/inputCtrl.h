#ifndef INPUT_CTRL_H_
#define INPUT_CTRL_H_

#include "inputProvider.h"
#include "inputCfg.h"

#define INPUT_SIZE 128

typedef enum {
    INPUT_RES_FINISHED,
    INPUT_RES_CANCELED,
    INPUT_RES_PROCESSING,
    INPUT_NO_ACTION

} InputResult;

typedef struct {

    InputResult (*handle)(InputEvent* ev, InputCfg cfg);

    bool (*validate)();

    const char* (*display)();

    void (*reset)();

} InputCb;

typedef struct {

    InputProvider* provider;

    InputCb* cb;

    InputCfg cfg;

    // pure non-formated input
    char input[INPUT_SIZE];
    // formated input based on input mode
    char finput[INPUT_SIZE];

} InputController;

InputResult inputUpdate(InputController* c, InputEvent* ev);

void inputInit(InputController* c);

extern InputController keypadCtrl;
extern InputController pedCtrl;

#endif